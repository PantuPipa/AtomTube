"""Exercise the real loopback callback and Windows DPAPI with a simulated Google account."""
import base64
import hashlib
import importlib.util
import json
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import Mock, patch
from urllib.error import HTTPError
from urllib.parse import parse_qs, urlencode, urlsplit
from urllib.request import urlopen

plugin = next((Path(__file__).resolve().parents[1] / 'tools').rglob('atomtube_home.py'))
spec = importlib.util.spec_from_file_location('home', plugin)
home = importlib.util.module_from_spec(spec)
with patch.dict(sys.modules, {'yt_dlp.extractor.common': Mock(InfoExtractor=object)}):
    spec.loader.exec_module(home)


class HomeTest(unittest.TestCase):
    def test_saved_playlist(self):
        def item(video, title, privacy='public'):
            return {'snippet': {'resourceId': {'videoId': video}, 'title': title,
                                'videoOwnerChannelTitle': 'Video owner', 'channelTitle': 'Playlist owner'},
                    'status': {'privacyStatus': privacy}}
        def api(resource, **params):
            if resource == 'playlists':
                self.assertEqual(params['mine'], 'true')
                if not params.get('pageToken'):
                    return {'items': [{'id': 'other', 'snippet': {'title': 'Assistir mais tarde'}}], 'nextPageToken': 'more'}
                return {'items': [{'id': 'own', 'snippet': {'title': ' Ver no  AtomTube '}}]}
            self.assertEqual((resource, params['playlistId'], params['maxResults']), ('playlistItems', 'own', 15))
            if params.get('pageToken') == 'second+page=':
                return {'items': [item('22222222222', 'B')], 'prevPageToken': 'first'}
            return {'items': [item('33333333333', 'C', 'unlisted'), item('44444444444', 'Restricted', 'private'),
                              item('11111111111', 'A')], 'nextPageToken': 'second+page='}
        with patch.object(home, 'account_identity', return_value=({}, 'test')), patch.object(home, 'youtube_api', return_value=api):
            rows, status = home.saved_playlist(None)
            self.assertEqual([r['title'] for r in rows], ['C', 'A'])
            self.assertEqual(rows[0]['channel'], 'Video owner')
            tag, prev, following = status.split('|')
            self.assertEqual((tag, prev), ('saved', ''))
            rows, status = home.saved_playlist(following)
            self.assertEqual([r['title'] for r in rows], ['B'])
            tag, prev, following = status.split('|')
            self.assertEqual(following, '')
            self.assertEqual(home.saved_playlist(prev)[0][0]['title'], 'C')
        with self.assertRaisesRegex(home.FeedError, 'no-playlist'):
            home.find_saved_playlist(lambda *a, **kw: {'items': []})
        with patch.object(home, 'account_identity', return_value=({}, 'test')), patch.object(home, 'find_saved_playlist', return_value='own'), patch.object(home, 'youtube_api', return_value=lambda *a, **kw: {'items': []}):
            self.assertEqual(home.saved_playlist(None), ([], 'saved||'))

    def test_account(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(home, 'ROOT', Path(tmp)):
            config = {'client_id': 'test.apps.googleusercontent.com', 'client_secret': 'test'}
            (home.ROOT / home.CLIENT).write_text(json.dumps({'installed': config}))
            auth = {}
            invalid = []
            threads = []

            def launch(hwnd, verb, address, *args):
                auth.update(parse_qs(urlsplit(address).query))
                self.assertEqual(auth['scope'], [home.SCOPE])
                self.assertEqual(auth['code_challenge_method'], ['S256'])
                target = auth['redirect_uri'][0]
                self.assertEqual(urlsplit(target).hostname, '127.0.0.1')

                def callback():
                    try:
                        urlopen(target + '?state=invalid&code=wrong', timeout=5)
                    except HTTPError as e:
                        invalid.append(e.code)
                    with urlopen(target + '?' + urlencode({'state': auth['state'][0], 'code': 'valid'}), timeout=5):
                        pass
                threads.append(threading.Thread(target=callback))
                threads[-1].start()
                return 33

            def issue(url, form):
                self.assertEqual(url, 'https://oauth2.googleapis.com/token')
                self.assertEqual(form['code'], 'valid')
                digest = hashlib.sha256(form['code_verifier'].encode()).digest()
                self.assertEqual(base64.urlsafe_b64encode(digest).decode().rstrip('='), auth['code_challenge'][0])
                return {'refresh_token': 'mock-refresh-token'}
            with patch.object(home.ctypes.windll.shell32, 'ShellExecuteW', launch), patch.object(home, 'request', issue):
                self.assertEqual(home.connect(), [])
            threads[0].join()
            self.assertEqual(invalid, [400])
            self.assertNotIn(b'mock-refresh-token', (home.ROOT / home.TOKEN).read_bytes())

            def google(url, form=None, token=None):
                if form:
                    self.assertEqual(form['refresh_token'], 'mock-refresh-token')
                    return {'access_token': 'mock-access'}
                self.assertEqual(token, 'mock-access')
                resource = urlsplit(url).path.split('/')[-1]
                if resource == 'subscriptions':
                    return {'items': [{'snippet': {'resourceId': {'channelId': 'channel'}}}]}
                if resource == 'channels':
                    return {'items': [{'id': 'channel', 'contentDetails': {'relatedPlaylists': {'uploads': 'uploads'}}}]}
                self.assertEqual(resource, 'playlistItems')
                return {'items': [{'snippet': {'title': 'Title', 'channelTitle': 'Author'},
                                   'contentDetails': {'videoId': '12345678901', 'videoPublishedAt': '2026-11-01'},
                                   'status': {'privacyStatus': 'public'}}]}
            with patch.object(home, 'request', google):
                self.assertEqual(home.subscriptions()[0]['id'], '12345678901')
            with patch.object(home, 'request', side_effect=AssertionError('expected cache')), patch.object(home, 'history', return_value=([], [['seen']])):
                with patch.object(home, 'recommendations', return_value=[{'id': 'local'}, {'id': 'seen'}, {'id': 'local'}]):
                    self.assertEqual([r['id'] for r in home.for_you(None, 'pt')], ['local', '12345678901'])
                    (home.ROOT / home.TOKEN).unlink()
                    self.assertEqual([r['id'] for r in home.for_you(None, 'pt')], ['local'])


if __name__ == '__main__':
    unittest.main()

