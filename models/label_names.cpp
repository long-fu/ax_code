#include "label_names.h"

#include <sstream>

namespace models
{

bool AssignLabelNames(const std::vector<std::string>& labels,
                      std::vector<detection::Object>& objects,
                      std::string& error)
{
    for (size_t i = 0; i < objects.size(); ++i)
    {
        const int label = objects[i].label;
        if (label < 0 || static_cast<size_t>(label) >= labels.size())
        {
            std::ostringstream message;
            message << "label index " << label << " at object " << i
                    << " is out of range [0, " << labels.size() << ')';
            error = message.str();
            return false;
        }
    }

    for (auto& object : objects)
        object.label_name = labels[static_cast<size_t>(object.label)];

    error.clear();
    return true;
}

} // namespace models
