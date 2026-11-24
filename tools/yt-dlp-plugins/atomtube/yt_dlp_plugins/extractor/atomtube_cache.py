"""AtomTube-only tuning for yt-dlp's bundled YouTube challenge provider."""

from yt_dlp.extractor.youtube.jsc._builtin.ejs import EJSBaseJCP
from yt_dlp.extractor.youtube._tab import YoutubeTabBaseInfoExtractor
from yt_dlp.utils import traverse_obj


# The upstream cache is disabled because yt-dlp cannot rotate large entries.
# AtomTube uses its own bounded cache directory and removes obsolete players.
EJSBaseJCP._ENABLE_PREPROCESSED_PLAYER_CACHE = True

# Keep the display strings and artwork already returned in a flat search.
_video = YoutubeTabBaseInfoExtractor._extract_video
_channel = YoutubeTabBaseInfoExtractor._extract_channel_renderer
_lockup = YoutubeTabBaseInfoExtractor._extract_lockup_view_model


def video(self, renderer):
    entry = _video(self, renderer)
    if entry:
        entry['at_date'] = self._get_text(renderer, 'publishedTimeText')
        entry['at_views'] = self._get_text(renderer, 'viewCountText')
        entry['at_avatar'] = traverse_obj(renderer, (
            'channelThumbnailSupportedRenderers', 'channelThumbnailWithLinkRenderer',
            'thumbnail', 'thumbnails', 0, 'url'))
    return entry


def channel(self, renderer):
    entry = _channel(self, renderer)
    if entry:
        for key in ('videoCountText', 'subscriberCountText'):
            label = self._get_text(renderer, key) or ''
            if any(word in label.lower() for word in ('inscrit', 'subscriber', 'suscrip')):
                entry['at_subs'] = label
                break
    return entry


def lockup(self, renderer):
    entry = _lockup(self, renderer)
    if entry:
        labels = traverse_obj(renderer, (
            'metadata', 'lockupMetadataViewModel', 'metadata', 'contentMetadataViewModel',
            'metadataRows', ..., 'metadataParts', ..., 'text', 'content', {str}))
        for label in labels:
            normalized = ' '.join(label.lower().split())
            if 'há ' in normalized or normalized.endswith(' ago') or 'hace ' in normalized:
                entry['at_date'] = label
            elif normalized[:1].isdigit() and len(normalized) < 50 and (
                    'visualiza' in normalized or 'vistas' in normalized or 'view' in normalized or
                    normalized.split()[-1] in ('mil', 'mi', 'milhão', 'milhões', 'bi')):
                entry['at_views'] = label
        entry['at_avatar'] = traverse_obj(renderer, (
            'metadata', 'lockupMetadataViewModel', 'image', 'decoratedAvatarViewModel',
            'avatar', 'avatarViewModel', 'image', 'sources', 0, 'url'))
        overlays = traverse_obj(renderer, (
            'contentImage', 'collectionThumbnailViewModel', 'primaryThumbnail',
            'thumbnailViewModel', 'overlays', ..., 'thumbnailOverlayBadgeViewModel',
            'thumbnailBadges', ..., 'thumbnailBadgeViewModel', 'text', {str}))
        entry['at_count'] = next((s for s in overlays if any(c.isdigit() for c in s)), None)
    return entry


YoutubeTabBaseInfoExtractor._extract_video = video
YoutubeTabBaseInfoExtractor._extract_channel_renderer = channel
YoutubeTabBaseInfoExtractor._extract_lockup_view_model = lockup

__all__ = ()
