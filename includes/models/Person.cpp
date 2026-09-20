#include "Person.h"
#include <sstream>

namespace models {

namespace {

// A deliberately permissive check: exactly one '@', with at least one character
// before it and a dotted domain after it. The table only requires NOT NULL and
// UNIQUE, so this just catches obvious nonsense before it reaches the database.
bool looksLikeEmail(const std::string &email)
{
    size_t at = email.find('@');
    if (at == std::string::npos || at == 0)
        return false;
    if (email.find('@', at + 1) != std::string::npos)
        return false;

    const std::string domain = email.substr(at + 1);
    size_t dot = domain.find('.');
    return dot != std::string::npos && dot != 0 && dot + 1 < domain.size();
}

// Both required string columns are checked the same way: present, and within
// the width the table declares.
bool checkRequiredText(const std::string &value,
                       size_t maxLength,
                       const char *field,
                       std::string &error)
{
    if (value.empty()) {
        error = std::string("'") + field + "' is required";
        return false;
    }
    if (value.size() > maxLength) {
        std::ostringstream message;
        message << "'" << field << "' must be at most " << maxLength << " characters";
        error = message.str();
        return false;
    }
    return true;
}

} // namespace

Person::Person()
{
}

Person::Person(const std::string &name, const std::string &email)
    : name_(name), email_(email)
{
}

picojson::value Person::toJson() const
{
    picojson::object obj;

    // Each field is added only when it holds a value, which is what keeps null
    // properties out of the response entirely.
    if (id_.hasValue())
        obj["id"] = picojson::value(static_cast<double>(id_.value()));

    obj["name"] = picojson::value(name_);
    obj["email"] = picojson::value(email_);

    if (address_.hasValue())
        obj["address"] = picojson::value(address_.value());

    return picojson::value(obj);
}

std::string Person::toJsonString() const
{
    return toJson().serialize();
}

bool Person::fromJson(const picojson::value &json, Person &out, std::string &error)
{
    if (!json.is<picojson::object>()) {
        error = "payload must be a JSON object";
        return false;
    }

    const picojson::object &obj = json.get<picojson::object>();
    Person parsed;

    picojson::object::const_iterator it = obj.find("id");
    if (it != obj.end() && !it->second.is<picojson::null>()) {
        if (!it->second.is<double>()) {
            error = "'id' must be a number";
            return false;
        }
        parsed.setId(static_cast<long long>(it->second.get<double>()));
    }

    it = obj.find("name");
    if (it != obj.end() && !it->second.is<picojson::null>()) {
        if (!it->second.is<std::string>()) {
            error = "'name' must be a string";
            return false;
        }
        parsed.setName(it->second.get<std::string>());
    }

    it = obj.find("email");
    if (it != obj.end() && !it->second.is<picojson::null>()) {
        if (!it->second.is<std::string>()) {
            error = "'email' must be a string";
            return false;
        }
        parsed.setEmail(it->second.get<std::string>());
    }

    // An absent "address" and an explicit null both mean "no address", which is
    // the round trip of what toJson() writes.
    it = obj.find("address");
    if (it != obj.end() && !it->second.is<picojson::null>()) {
        if (!it->second.is<std::string>()) {
            error = "'address' must be a string";
            return false;
        }
        parsed.setAddress(it->second.get<std::string>());
    }

    out = parsed;
    return true;
}

bool Person::validate(std::string &error) const
{
    if (!checkRequiredText(name_, MAX_NAME_LENGTH, "name", error))
        return false;

    if (!checkRequiredText(email_, MAX_EMAIL_LENGTH, "email", error))
        return false;

    if (!looksLikeEmail(email_)) {
        error = "'email' is not a valid email address";
        return false;
    }

    // The address is optional, so only its width is constrained.
    if (address_.hasValue() && address_.value().size() > MAX_ADDRESS_LENGTH) {
        std::ostringstream message;
        message << "'address' must be at most " << MAX_ADDRESS_LENGTH << " characters";
        error = message.str();
        return false;
    }

    error.clear();
    return true;
}

} // namespace models
