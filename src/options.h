#pragma once
enum class ImageQuality { Original, High, Medium, Low, Potato };
enum class Language { Portuguese, English, Spanish };
constexpr unsigned frameTargets[]={0,60,50,40,35,30,24,17};
struct PlaybackOptions {
    ImageQuality image=ImageQuality::Medium;
    unsigned fps=24;
    Language language=Language::Portuguese;
    const wchar_t* audioLanguage() const {
        return language==Language::Portuguese ? L"pt-BR" : language==Language::English ? L"en" : L"es";
    }
    const wchar_t* catalogLanguage() const {
        return language==Language::Portuguese ? L"pt" : audioLanguage();
    }
};
