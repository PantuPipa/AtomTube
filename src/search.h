#pragma once
#include "common.h"
#include <memory>
#include <vector>

struct SearchResult {
    wstring id, title, duration;
    std::shared_ptr<void> thumbnail;
    wstring author,channel,date,views,subscribers,description,count,imageUrl,avatarUrl,live;
    bool verified=false;
    std::shared_ptr<void> avatar;
};
enum class SearchKind { Videos, Channels, Uploads, Playlists, Streams, ChannelPlaylists, Entries, ChannelSearch, Preview, ForYou, Subscriptions, Connect, SavedVideos };
inline bool channelPage(SearchKind kind) {
    return kind==SearchKind::Uploads || kind==SearchKind::Streams || kind==SearchKind::ChannelPlaylists || kind==SearchKind::ChannelSearch;
}
inline bool playlistRows(SearchKind kind) { return kind==SearchKind::Playlists || kind==SearchKind::ChannelPlaylists; }
struct ChannelHeader {
    wstring name,avatarUrl,bannerUrl;
    std::shared_ptr<void> avatar,banner;
};
class YoutubeSearch {
    struct Task;
    std::shared_ptr<Task> task;
public:
    ~YoutubeSearch() { cancel(); }
    void start(SearchKind kind,const wstring& query,unsigned first,const std::vector<SearchResult>& previous,const wchar_t* language=L"pt");
    void cancel();
    // Returns false after the worker has delivered its last update.
    bool poll(std::vector<SearchResult>& results,wstring& status,unsigned& next,ChannelHeader* header=nullptr);
};
