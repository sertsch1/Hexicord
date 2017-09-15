#ifndef HEXICORD_TYPES_IMAGE_HPP
#define HEXICORD_TYPES_IMAGE_HPP

#include <vector>
#include <string>
#include <hexicord/exceptions.hpp>
#include <hexicord/types/snowflake.hpp>
#include <hexicord/types/file.hpp>

namespace boost { namespace asio { class io_service; }}

namespace Hexicord {

    /**
     * Image formats supported by API.
     */
    enum ImageFormat {
        Detect = 0,

        Jpeg = 1<<1,
        Png  = 1<<2,
        Webp = 1<<3,
        Gif  = 1<<4,
    };

    /**
     * Container for (format, file) pair.
     */
    struct Image {
        /**
         * Construct new image instance.
         *
         * If format is Detect and detection failed, LogicError
         * will be thrown.
         */
        Image(const File& file, ImageFormat format = Detect);

        /**
         * Forced or detected (if Detect passed in c-tor) format.
         */
        const ImageFormat format;
        const File file;

    private:
        ImageFormat detectFormat(const File& file);
    };

    enum ImageType {
        CustomEmoji       = (1<<5 ) | ImageFormat::Png,
        GuildIcon         = (1<<6 ) | ImageFormat::Png | ImageFormat::Jpeg | ImageFormat::Webp,
        GuildSplash       = (1<<7 ) | ImageFormat::Png | ImageFormat::Jpeg | ImageFormat::Webp,
        DefaultUserAvatar = (1<<8 ) | ImageFormat::Png,
        UserAvatar        = (1<<9 ) | ImageFormat::Png | ImageFormat::Jpeg | ImageFormat::Webp | ImageFormat::Gif,
        ApplicationIcon   = (1<<10) | ImageFormat::Png | ImageFormat::Jpeg | ImageFormat::Webp,
    };
    // ^ enum values contain bitmask of supported formats, first element added
    // to make sure enum don't have same value.

    /**
     * Implementation details. Probably not what you looking for.
     */
    namespace _detail {
        std::vector<uint8_t> cdnDownload(boost::asio::io_service& ioService, const std::string& path);

        inline constexpr bool isPowerOfTwo(unsigned number) {
            return ((number != 0) && ((number & (~number + 1)) == number));
        }

        inline constexpr bool isSupportedFormat(ImageType type, ImageFormat format) {
            return (type & format) == format;
        }

        template<ImageType>
        inline std::string basePath() {}

        template<> inline std::string basePath<CustomEmoji>()       { return "/emojis";         }
        template<> inline std::string basePath<GuildIcon>()         { return "/icons";          }
        template<> inline std::string basePath<GuildSplash>()       { return "/splashes";       }
        template<> inline std::string basePath<DefaultUserAvatar>() { return "/embed/avatars";  }
        template<> inline std::string basePath<UserAvatar>()        { return "/avatars";        }
        template<> inline std::string basePath<ApplicationIcon>()   { return "/app-icons";      }

        template<ImageFormat>
        inline std::string formatExtension() {}

        template<> inline std::string formatExtension<Jpeg>() { return "jpg";  }
        template<> inline std::string formatExtension<Png>()  { return "png";  }
        template<> inline std::string formatExtension<Webp>() { return "webp"; }
        template<> inline std::string formatExtension<Gif>()  { return "gif";  }
    } // namespace _detail

    /**
     * Reference to remote image.
     */
    template<ImageType Type>
    struct ImageReference {
        inline ImageReference(Snowflake id, const std::string& hash)
            : id(id)
            , hash(hash) {}

        template<ImageFormat Format>
        inline std::string url(unsigned short size) const {
            // Construct path like this:
            //   {base_path}/{id}/{hash}.{format_extension}?size={size}
            //   avatars/339355417366888458/35fa476e4898faf740cc48edb989f20c.png?size=2048
            return _detail::basePath<Type>() + "/" + std::to_string(id) + "/" +
                   hash + "." + _detail::formatExtension<Format>() +
                   "?size=" + std::to_string(size);
        }

        template<ImageFormat Format>
        inline Image download(boost::asio::io_service& ioService, unsigned short size) const {
            static_assert(_detail::isSupportedFormat(Type, Format), "Format is not supported for this image type.");
            if (!_detail::isPowerOfTwo(size)) throw LogicError("Image size must be power of two.", -1);

            return Image(File(hash + "." + _detail::formatExtension<Format>(),
                              _detail::cdnDownload(ioService, url<Format>(size))),
                         Format);
        }

        const Snowflake id;
        const std::string hash;
    };

    template<>
    struct ImageReference<CustomEmoji> {
        inline ImageReference(const std::string& hash)
            : hash(hash) {}

        template<ImageFormat Format>
        inline std::string url(unsigned short size) const {
            // Construct path like this:
            //   {base_path}/{id}/{hash}.{format_extension}?size={size}
            //   avatars/339355417366888458/35fa476e4898faf740cc48edb989f20c.png?size=2048
            return _detail::basePath<CustomEmoji>() + "/" +
                   hash + "." + _detail::formatExtension<Format>() +
                   "?size=" + std::to_string(size);
        }

        template<ImageFormat Format>
        inline Image download(boost::asio::io_service& ioService, unsigned short size) const {
            static_assert(_detail::isSupportedFormat(CustomEmoji, Format), "Format is not supported for this image type.");
            if (!_detail::isPowerOfTwo(size)) throw LogicError("Image size must be power of two.", -1);

            return Image(File(hash + "." + _detail::formatExtension<Format>(),
                              _detail::cdnDownload(ioService, url<Format>(size))),
                         Format);
        }

        const std::string hash;
    };
   
    template<>
    struct ImageReference<DefaultUserAvatar> {
        inline ImageReference(const int userDiscriminator)
            : userDiscriminator(userDiscriminator) {}

        template<ImageFormat Format>
        inline std::string url(unsigned short size) const {
            // Construct path like this:
            //   {base_path}/{id}/{hash}.{format_extension}?size={size}
            //   avatars/339355417366888458/35fa476e4898faf740cc48edb989f20c.png?size=2048
            return _detail::basePath<DefaultUserAvatar>() + "/" +
                   std::to_string(userDiscriminator % 5) + "." + _detail::formatExtension<Format>() +
                   "?size=" + std::to_string(size);
        }

        template<ImageFormat Format>
        inline Image download(boost::asio::io_service& ioService, unsigned short size) const {
            static_assert(_detail::isSupportedFormat(CustomEmoji, Format), "Format is not supported for this image type.");
            if (!_detail::isPowerOfTwo(size)) throw LogicError("Image size must be power of two.", -1);

            return Image(File(std::to_string(userDiscriminator % 5) + "." + _detail::formatExtension<Format>(),
                              _detail::cdnDownload(ioService, url<Format>(size))),
                         Format);
        }

        const int userDiscriminator;
    };

} // namespace Hexicord

#endif // HEXICORD_TYPES_IMAGE_HPP
