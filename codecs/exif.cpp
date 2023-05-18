#include "exif.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <libexif/exif-data.h>

namespace exif
{
    std::optional<Orientation> get_orientation(const unsigned char * data , std::size_t len)
    {
        auto orientation = std::optional<Orientation>{};
        auto exif_data = exif_data_new_from_data(data, len);
        if(exif_data)
        {
            auto orientation_entry = exif_data_get_entry(exif_data, EXIF_TAG_ORIENTATION);
            if(orientation_entry && orientation_entry->format == EXIF_FORMAT_SHORT)
            {
                orientation = static_cast<Orientation>(exif_get_short(orientation_entry->data, exif_data_get_byte_order(exif_data)));
                if(orientation != Orientation::r_0 && orientation != Orientation::r_180 && orientation != Orientation::r_270 && orientation != Orientation::r_90)
                {
                    exif_data_unref(exif_data);
                    std::vector<char> desc(256);
                    exif_entry_get_value(orientation_entry, std::data(desc), std::size(desc));
                    throw std::runtime_error{"Unsupported EXIF rotation: " + std::string{std::data(desc)} + " (" + std::to_string(static_cast<std::underlying_type_t<Orientation>>(*orientation)) + ")"};
                }
            }
            exif_data_unref(exif_data);
        }
        return orientation;
    }
}
