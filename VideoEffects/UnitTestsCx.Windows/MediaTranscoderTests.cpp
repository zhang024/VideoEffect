#include "pch.h"

using namespace concurrency;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Lumia::Imaging;
using namespace Lumia::Imaging::Artistic;
using namespace Lumia::Imaging::Transforms;
using namespace Platform;
using namespace Platform::Collections;
using namespace VideoEffects;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Transcoding;
using namespace Windows::Storage;
using namespace Windows::Storage::FileProperties;

// Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
//namespace UnitTests
//{
//    public ref class ExampleFilterChain sealed : IFilterChainFactory
//    {
//    public:
//        virtual IIterable<IFilter^>^ Create()
//        {
//            auto filters = ref new Vector<IFilter^>();
//            filters->Append(ref new AntiqueFilter());
//            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
//            return filters;
//        }
//    };
//}
//using namespace UnitTests;

ref class AnimatedWarp sealed : public IAnimatedFilterChain
{
public:

    virtual property IIterable<IFilter^>^ Filters;

    virtual void UpdateTime(TimeSpan time)
    {
        _filter->Level = .5 * (sin(2. * M_PI * time.Duration / 10000000.) + 1); // 1Hz oscillation between 0 and 1
    }

    AnimatedWarp()
    {
        _filter = ref new WarpFilter(WarpEffect::Twister, 0.);
        auto filters = ref new Vector<IFilter^>();
        filters->Append(_filter);
        Filters = filters;
    }

private:
    WarpFilter^ _filter;
};

TEST_CLASS(MediaTranscoderTests)
{
public:
    TEST_METHOD(CX_W_MT_Basic)
    {
        TestBasic(L"Car.mp4", L"CX_W_MT_Basic.mp4");
    }

    TEST_METHOD(CX_W_MT_Basic_RotatedVideo)
    {
        TestBasic(L"OriginalR.mp4", L"CX_W_MT_Basic_RotatedVideo.mp4");
    }

    void TestBasic(String^ inputFileName, String^ outputFileName)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/" + inputFileName)));
        StorageFile^ destination = CreateDestinationFile(outputFileName);

        // Note: this does not work - ExampleFilterChain must be in a separate WinRT DLL
        // auto definition = ref new LumiaEffectDefinition(ExampleFilterChain().GetType()->FullName);

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new AntiqueFilter());
            filters->Append(ref new FlipFilter(FlipMode::Horizontal));
            return filters;
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_LumiaCropSquare)
    {
        TestLumiaCropSquare(L"Car.mp4", L"CX_W_MT_LumiaCropSquare.mp4");
    }

    TEST_METHOD(CX_W_MT_LumiaCropSquare_RotatedVideo)
    {
        TestLumiaCropSquare(L"OriginalR.mp4", L"CX_W_MT_LumiaCropSquare_RotatedVideo.mp4");
    }

    void TestLumiaCropSquare(String^ inputFileName, String^ outputFileName)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/" + inputFileName)));
        StorageFile^ destination = CreateDestinationFile(outputFileName);

        // Select the largest centered square area in the input video
        auto encodingProfile = Await(TranscodingProfile::CreateFromFileAsync(source));
        unsigned int inputWidth = encodingProfile->Video->Width;
        unsigned int inputHeight = encodingProfile->Video->Height;
        unsigned int outputLength = min(inputWidth, inputHeight);
        Rect cropArea(
            (float)((inputWidth - outputLength) / 2),
            (float)((inputHeight - outputLength) / 2),
            (float)outputLength,
            (float)outputLength
            );
        encodingProfile->Video->Width = outputLength;
        encodingProfile->Video->Height = outputLength;

        auto definition = ref new LumiaEffectDefinition(ref new FilterChainFactory([cropArea]()
        {
            auto filters = ref new Vector<IFilter^>();
            filters->Append(ref new CropFilter(cropArea));
            return filters;
        }));
        definition->InputWidth = inputWidth;
        definition->InputHeight = inputHeight;
        definition->OutputWidth = outputLength;
        definition->OutputHeight = outputLength;

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, encodingProfile));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_Square)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_Square_bad.mp4");

        // Compute the output resolution
        auto encodingProfile = Await(TranscodingProfile::CreateFromFileAsync(source));
        unsigned int inputWidth = encodingProfile->Video->Width;
        unsigned int inputHeight = encodingProfile->Video->Height;
        unsigned int outputLength = min(inputWidth, inputHeight);
        encodingProfile->Video->Width = outputLength;
        encodingProfile->Video->Height = outputLength;

        auto definition = ref new SquareEffectDefinition();

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, encodingProfile));
        Await(transcode->TranscodeAsync());
    }

    TEST_METHOD(CX_W_MT_AnimatedWarp)
    {
        StorageFile^ source = Await(StorageFile::GetFileFromApplicationUriAsync(ref new Uri(L"ms-appx:///Input/Car.mp4")));
        StorageFile^ destination = CreateDestinationFile(L"CX_W_MT_AnimatedWarp.mp4");

        auto definition = ref new LumiaEffectDefinition(ref new AnimatedFilterChainFactory([]()
        {
            return ref new AnimatedWarp();
        }));

        auto transcoder = ref new MediaTranscoder();
        transcoder->AddVideoEffect(definition->ActivatableClassId, true, definition->Properties);

        PrepareTranscodeResult^ transcode = Await(transcoder->PrepareFileTranscodeAsync(source, destination, MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Qvga)));
        Await(transcode->TranscodeAsync());
    }

    StorageFile^ CreateDestinationFile(String^ filename)
    {
        StorageFolder^ folder = Await(KnownFolders::VideosLibrary->CreateFolderAsync(L"UnitTests.VideoEffects", CreationCollisionOption::OpenIfExists));
        return Await(folder->CreateFileAsync(filename, CreationCollisionOption::ReplaceExisting));
    }
};