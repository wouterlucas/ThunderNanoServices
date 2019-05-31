#include "PlayerImplementation.h"
#include <gst/gst.h>
#include <priv_aamp.h>

namespace WPEFramework {

namespace Player {

    namespace Implementation {

        class Config : public Core::JSON::Container {
        private:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

        public:
            Config()
                : Core::JSON::Container()
                , Speeds()
            {
                Add(_T("speeds"), &Speeds);
            }
            ~Config()
            {
            }

        public:
            Core::JSON::ArrayType<Core::JSON::DecSInt32> Speeds;
        };

        string PlayerPlatform::_configuration = "";

        PlayerPlatform::PlayerPlatform(const Exchange::IStream::streamtype type, const uint8_t index, ICallback* callbacks)
            : _uri("")
            , _state(Exchange::IStream::Idle)
            , _drmType(Exchange::IStream::Unknown)
            , _streamType(type)
            , _speed(0)
            , _begin(0)
            , _end(~0)
            , _z(0)
            , _rectangle()
            , _callback(callbacks)
            , _initialized(false)
            , _aampGstPlayerMainLoop(nullptr)
        {

            Config config;
            config.FromString(_configuration);

            if ((config.Speeds.IsSet() == true) && (config.Speeds.Length() != 0)) {
                Core::JSON::ArrayType<Core::JSON::DecSInt32>::Iterator index(config.Speeds.Elements());
                while (index.Next() == true) {
                    _speeds.push_back(index.Current().Value());
                }
            }
            else {
                int32_t speeds[] = { 100, -100, 200, -200, 400, -400, 800, -800, 1600, -1600, 3200, -3200};
                _speeds.assign(std::begin(speeds), std::end(speeds));
            }
            _rectangle.X = 0;
            _rectangle.Y = 0;
            _rectangle.Width = 1080;
            _rectangle.Height = 720;

            Core::SystemInfo::SetEnvironment(_T("PLAYERSINKBIN_USE_WESTEROSSINK"), _T("true"));
            InitializePlayerInstance();

            _aampPlayer = new PlayerInstanceAAMP();

            _state = Exchange::IStream::Prepared;
            Run();
        }

        PlayerPlatform::~PlayerPlatform()
        {
            Terminate();
            _speeds.clear();
        }

        void PlayerPlatform::InitializePlayerInstance()
        {
            if (!_initialized) {
                _initialized = true;
                gst_init(0, nullptr);
                _aampGstPlayerMainLoop = g_main_loop_new(nullptr, false);
            }
        }

        void PlayerPlatform::DeinitializePlayerInstance()
        {
            if (_initialized == true) {
                if (_aampGstPlayerMainLoop)
                    g_main_loop_quit(_aampGstPlayerMainLoop);
                _initialized = false;
            }
        }

        void PlayerPlatform::Window(const Rectangle& rectangle)
        {
            _rectangle = rectangle;
            _aampPlayer->SetVideoRectangle(_rectangle.X, _rectangle.Y, _rectangle.Width, _rectangle.Height);
        }

        uint32_t PlayerPlatform::Load(const string& uri)
        {
            uint32_t result = Core::ERROR_NONE;
            TRACE(Trace::Information, (string(__FUNCTION__)));
            TRACE(Trace::Information, (_T("uri = %s"), uri.c_str()));

            if (uri.empty() == false) {
                TRACE(Trace::Information, (_T("uri = %s"), uri.c_str()));
                string uriType = UriType(uri);
                if ((uriType == "m3u8") || (uriType == "mpd")) {
                    TRACE(Trace::Information, (_T("URI type is %s"), uriType.c_str()));

                    _uri = uri;
                    _aampPlayer->Tune(_uri.c_str());
                    _aampPlayer->SetRate(0);
                } else {
                    result = Core::ERROR_INCORRECT_URL;
                    _state = Exchange::IStream::Error;
                    TRACE(Trace::Error, (_T("URI is not dash/hls")));
                }
            } else {
                result = Core::ERROR_INCORRECT_URL;
                TRACE(Trace::Error, (_T("URI is not provided")));
            }

            return result;
        }

        uint64_t PlayerPlatform::Position() const
        {
            return (_aampPlayer->GetPlaybackPosition() * 1000);
        }

        void PlayerPlatform::Position(const uint64_t absoluteTime)
        {
            return _aampPlayer->Seek(absoluteTime/1000);
        }

        void PlayerPlatform::Terminate()
        {
            _aampPlayer->Stop();
            Block();

            delete _aampPlayer;
            DeinitializePlayerInstance();

            TRACE(Trace::Information, (string(__FUNCTION__)));
            Wait(Thread::BLOCKED | Thread::STOPPED, Core::infinite);
        }

        uint32_t PlayerPlatform::Worker()
        {
            TRACE(Trace::Information, (string(__FUNCTION__)));
            if (_aampGstPlayerMainLoop) {
                g_main_loop_run(_aampGstPlayerMainLoop); // blocks
                TRACE(Trace::Information, (string(__FUNCTION__)));
            }
            g_main_loop_unref(_aampGstPlayerMainLoop);
            _aampGstPlayerMainLoop = nullptr;
            return WPEFramework::Core::infinite;
        }

        uint32_t PlayerPlatform::Speed(const int32_t speed)
        {
            TRACE(Trace::Information, (string(__FUNCTION__)));
            TRACE(Trace::Information, (_T("speed = %d"), speed));
            uint32_t result = Core::ERROR_NONE;

            if (speed != _speed) {

                Exchange::IStream::state newState = _state;

                if (speed != 0) {
                    SpeedList::iterator index =  std::find(_speeds.begin(), _speeds.end(), speed);
                    if (index != _speeds.end()) {
                        newState = Exchange::IStream::Playing;
                    } else {
                        result = Core::ERROR_BAD_REQUEST;
                    }

                } else {
                    newState = Exchange::IStream::Paused;
                }
                _speed = speed;

                _aampPlayer->SetRate((speed/100));
                if (_state != newState) {
                    _state = newState;
                    if (_callback != nullptr) {
                       _callback->StateChange(_state);
                    }
                }
            }
            return result;
       }
    }
}
} //namespace WPEFramework::Player::Implementation
