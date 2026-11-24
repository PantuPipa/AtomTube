"""Read-only feed provider bundled with the AtomTube desktop application."""
import base64
import ctypes
import hashlib
import itertools
import json
import os
import re
import sys
import tempfile
import time
import winreg
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlencode, urlsplit
from urllib.request import Request, urlopen

from yt_dlp.extractor.common import InfoExtractor

ROOT = Path(os.environ['LOCALAPPDATA']) / 'AtomTube'
CLIENT = 'google-oauth.json'
TOKEN = 'youtube-account.bin'
SCOPE = 'https://www.googleapis.com/auth/youtube.readonly'
STARTERS = {
    'pt': ('experimentos de ciências para estudantes', 'astronomia curiosidades universo'),
    'en': ('science experiments for students', 'astronomy space discoveries'),
    'es': ('experimentos de ciencias para estudiantes', 'astronomía curiosidades del universo'),
}


def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8-sig'))
    except (ValueError, OSError):
        return {}


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temp = Path(stream.name)
        try:
            stream.write(data)
        except BaseException:
            stream.close()
            temp.unlink(missing_ok=True)
            raise
    try:
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)


def protect(data, decrypt=False):
    class Blob(ctypes.Structure):
        _fields_ = [('size', ctypes.c_uint32), ('data', ctypes.c_void_p)]
    buf = ctypes.create_string_buffer(data)
    src, dst = Blob(len(data), ctypes.addressof(buf)), Blob()
    crypto = ctypes.windll.crypt32
    fn = crypto.CryptUnprotectData if decrypt else crypto.CryptProtectData
    if not fn(ctypes.byref(src), None, None, None, None, 1, ctypes.byref(dst)):
        raise OSError('DPAPI')
    try:
        return ctypes.string_at(dst.data, dst.size)
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.c_void_p(dst.data))


def client_config():
    for path in (ROOT / CLIENT, Path(sys.executable).resolve().parent.parent / CLIENT):
        candidate = read_json(path)
        installed = candidate.get('installed') if isinstance(candidate, dict) else None
        if not isinstance(installed, dict) or not isinstance(installed.get('client_id'), str):
            continue
        if re.fullmatch(r'[a-zA-Z0-9_-]+\.apps\.googleusercontent\.com', installed.get('client_id', '')):
            return {k: installed[k] for k in ('client_id', 'client_secret') if installed.get(k)}
    raise FeedError('config')


class FeedError(Exception):
    pass


def request(url, form=None, token=None):
    req = Request(url, data=urlencode(form).encode() if form else None)
    if token:
        req.add_header('Authorization', 'Bearer ' + token)
    try:
        with urlopen(req, timeout=8) as response:
            return json.load(response)
    except HTTPError as e:
        try:
            err = json.load(e).get('error')
        except ValueError:
            err = None
        if e.code == 404:
            raise FeedError('missing') from None
        if err == 'invalid_grant' or e.code == 401:
            raise FeedError('reauth') from None
        raise FeedError('api' if e.code in (400, 403) else 'network') from None


def random_token():
    return base64.urlsafe_b64encode(os.urandom(40)).decode().rstrip('=')


def connect():
    config = client_config()
    verifier, state = random_token(), random_token()
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).decode().rstrip('=')
    answer = {}

    class Callback(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            path = urlsplit(self.path)
            args = parse_qs(path.query)
            ok = path.path == '/callback' and args.get('state') == [state]
            if ok:
                answer.update(code=args.get('code', [''])[0])
            self.send_response(200 if ok else 400)
            self.send_header('Content-Type', 'text/plain; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(b'AtomTube - You may close this tab.' if ok else b'Invalid callback.')

    with HTTPServer(('127.0.0.1', 0), Callback) as server:
        server.timeout = 1
        redirect = 'http://127.0.0.1:%d/callback' % server.server_port
        url = 'https://accounts.google.com/o/oauth2/v2/auth?' + urlencode({
            'client_id': config['client_id'], 'redirect_uri': redirect, 'response_type': 'code',
            'scope': SCOPE, 'state': state, 'code_challenge': challenge,
            'code_challenge_method': 'S256', 'access_type': 'offline', 'prompt': 'consent select_account'})
        if ctypes.windll.shell32.ShellExecuteW(None, 'open', url, None, None, 1) <= 32:
            raise FeedError('browser')
        deadline = time.monotonic() + 165
        while not answer and time.monotonic() < deadline:
            server.handle_request()
    if not answer.get('code'):
        raise FeedError('cancel')
    tokens = request('https://oauth2.googleapis.com/token', {
        **config, 'code': answer['code'], 'code_verifier': verifier, 'redirect_uri': redirect,
        'grant_type': 'authorization_code'})
    refresh = tokens.get('refresh_token')
    if not refresh:
        raise FeedError('reauth')
    save(ROOT / TOKEN, protect(json.dumps({'refresh': refresh, 'client': config['client_id']}).encode()))
    return []


def account_identity():
    config = client_config()
    try:
        identity = json.loads(protect((ROOT / TOKEN).read_bytes(), decrypt=True))
    except (ValueError, OSError):
        raise FeedError('reauth') from None
    if identity['client'] != config['client_id']:
        raise FeedError('reauth')
    return config, identity['refresh']


def youtube_api(config, refresh):
    access = request('https://oauth2.googleapis.com/token', {
        **config, 'refresh_token': refresh, 'grant_type': 'refresh_token'})['access_token']

    def api(resource, **params):
        return request('https://www.googleapis.com/youtube/v3/' + resource + '?' + urlencode(params), token=access)

    return api


def subscriptions():
    config, refresh = account_identity()
    key = hashlib.sha256(refresh.encode()).hexdigest()
    cache = read_json(ROOT / 'subscriptions.json')
    if cache.get('key') == key and time.time() - cache.get('time', 0) < 3600:
        return cache['rows']
    api = youtube_api(config, refresh)
    # YouTube orders these channels by recent activity. Bound calls on slow computers.
    subs = api('subscriptions', part='snippet', mine='true', order='unread', maxResults=12)
    ids = [s['snippet']['resourceId']['channelId'] for s in subs.get('items', [])]
    channels = api('channels', part='contentDetails', id=','.join(ids)) if ids else {}
    rows = {}
    for ch in channels.get('items', []):
        playlist = ch['contentDetails']['relatedPlaylists']['uploads']
        try:
            uploads = api('playlistItems', part='snippet,contentDetails,status', playlistId=playlist, maxResults=4)
        except FeedError as e:
            if str(e) == 'missing':
                continue
            raise
        for item in uploads.get('items', []):
            s, detail = item['snippet'], item['contentDetails']
            if item.get('status', {}).get('privacyStatus') != 'public' or not detail.get('videoPublishedAt'):
                continue
            vid = detail['videoId']
            rows[vid] = dict(id=vid, title=s['title'], channel_id=ch['id'], channel=s['channelTitle'],
                             published=detail['videoPublishedAt'])
    rows = sorted(rows.values(), key=lambda r: r['published'], reverse=True)[:15]
    save(ROOT / 'subscriptions.json', json.dumps(dict(key=key, time=time.time(), rows=rows)).encode())
    return rows


def find_saved_playlist(api):
    params = dict(part='snippet', mine='true', maxResults=50,
                  fields='nextPageToken,items(id,snippet/title)')
    while True:
        page = api('playlists', **params)
        for entry in page.get('items', []):
            if ' '.join(entry['snippet']['title'].split()).casefold() == 'ver no atomtube':
                return entry['id']
        if not page.get('nextPageToken'):
            raise FeedError('no-playlist')
        params['pageToken'] = page['nextPageToken']


def saved_playlist(cursor):
    api = youtube_api(*account_identity())
    params = dict(part='snippet,status', maxResults=15,
                  fields='nextPageToken,prevPageToken,items(status/privacyStatus,snippet(title,videoOwnerChannelId,videoOwnerChannelTitle,resourceId/videoId))')
    if cursor:
        playlist, token = json.loads(base64.urlsafe_b64decode(cursor + '=' * (-len(cursor) % 4)))
        params['pageToken'] = token
    else:
        playlist = find_saved_playlist(api)
    page = api('playlistItems', playlistId=playlist, **params)
    rows = []
    for item in page.get('items', []):
        snippet = item.get('snippet', {})
        vid = snippet.get('resourceId', {}).get('videoId', '')
        if item.get('status', {}).get('privacyStatus') not in ('public', 'unlisted'):
            continue
        if re.fullmatch(r'[A-Za-z0-9_-]{11}', vid) and snippet.get('title'):
            rows.append(dict(id=vid, title=snippet['title'], channel=snippet.get('videoOwnerChannelTitle'),
                             channel_id=snippet.get('videoOwnerChannelId')))

    def cursor_for(field):
        if not page.get(field):
            return ''
        data = json.dumps([playlist, page[field]]).encode()
        return base64.urlsafe_b64encode(data).decode().rstrip('=')

    return rows, '|'.join(('saved', cursor_for('prevPageToken'), cursor_for('nextPageToken')))


def history():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\AtomTube') as key:
            def strings(name):
                try:
                    values, kind = winreg.QueryValueEx(key, name)
                    return values if kind == winreg.REG_MULTI_SZ else []
                except OSError:
                    return []
            return strings('Searches'), [s.split('\t') for s in strings('HomeWatched')]
    except OSError:
        return [], []


def recommendations(ie, lang):
    searches, watched = history()
    seen = {s[0] for s in watched if s}
    channels = list(dict.fromkeys(s[1] for s in watched if len(s) > 1 and s[1]))
    # Give the most recent interest a slot and rotate a second one across days.
    day = int(time.time() // 86400)
    topics = channels[:1] + searches[:1]
    alternatives = list(dict.fromkeys(searches[1:6] + channels[1:6]))
    if alternatives:
        topics.insert(1 if day % 2 else 2, alternatives[day % len(alternatives)])
    topics = list(dict.fromkeys(t.strip()[:140] for t in topics + list(STARTERS[lang]) if t.strip()))[:2]
    path = ROOT / ('home-' + lang + '.json')
    cached = read_json(path)
    if cached.get('topics') == topics and time.time() - cached.get('time', 0) < 14400:
        return [r for r in cached['rows'] if r['id'] not in seen]
    batches = []
    for term in topics:
        try:
            info = ie._downloader.extract_info('ytsearch10:' + term, download=False, process=False)
            entries = itertools.islice(info['entries'], 10)
            batches.append([{k: r.get(k) for k in ('id', 'title', 'channel', 'channel_id', 'duration')}
                            for r in entries if r and r.get('id') and r.get('title')])
        except Exception:
            continue
    rows = []
    for pair in itertools.zip_longest(*batches):
        for row in pair:
            if row and row['id'] not in seen:
                rows.append(row)
                seen.add(row['id'])
    if rows:
        save(path, json.dumps(dict(time=time.time(), topics=topics, rows=rows)).encode())
        return rows
    older = [r for r in cached.get('rows', []) if r['id'] not in seen]
    if older:
        return older
    raise FeedError('network')


def for_you(ie, lang):
    # Keep account-derived videos out of the local recommendations cache.
    try:
        local = recommendations(ie, lang)
    except FeedError:
        local = []
    followed = []
    if (ROOT / TOKEN).is_file():
        try:
            followed = subscriptions()
        except (FeedError, OSError, ValueError, KeyError):
            pass
    _, watched = history()
    seen = {s[0] for s in watched if s}
    result = []
    local = iter(local)
    for group in itertools.zip_longest(local, local, followed):
        for row in group:
            if row and row['id'] not in seen:
                seen.add(row['id'])
                result.append(row)
    if not result:
        raise FeedError('network')
    return result


class AtomTubeHomeIE(InfoExtractor):
    _VALID_URL = r'atomtubehome:(?P<id>connect|subscriptions|foryou|saved):(?P<lang>pt|en|es)(?::(?P<cursor>[A-Za-z0-9_-]+))?$'

    def _real_extract(self, url):
        action, lang, cursor = self._match_valid_url(url).group('id', 'lang', 'cursor')
        status = 'ok'
        try:
            if action == 'saved':
                rows, status = saved_playlist(cursor)
            else:
                rows = connect() if action == 'connect' else subscriptions() if action == 'subscriptions' else for_you(self, lang)
        except FeedError as e:
            rows, status = [], str(e)
        except Exception:
            rows, status = [], 'network'
        entries = [dict(r, _type='url', url='https://www.youtube.com/watch?v=' + r['id'], ie_key='Youtube') for r in rows]
        if action == 'foryou' and status == 'ok':
            searches, watched = history()
            if not searches and not watched and not (ROOT / TOKEN).is_file():
                status = 'starter'
        return self.playlist_result(entries, action, status)

