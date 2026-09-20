#ifndef __MODELS_PERSON_H__
#define __MODELS_PERSON_H__

#include <string>
#include <picojson.h>
#include <helpers/Nullable.h>

namespace models {

// Mirrors the `person` table:
//
//   id      INTEGER      PRIMARY KEY AUTOINCREMENT
//   name    VARCHAR(100) NOT NULL
//   email   VARCHAR(100) NOT NULL UNIQUE
//   address VARCHAR(255)              -- nullable
class Person
{
public:
    // Column widths the table declares, enforced by validate().
    static const size_t MAX_NAME_LENGTH = 100;
    static const size_t MAX_EMAIL_LENGTH = 100;
    static const size_t MAX_ADDRESS_LENGTH = 255;

    Person();
    Person(const std::string &name, const std::string &email);

    // Absent until the row has been inserted and given an autoincrement id.
    const Nullable<long long> &id() const { return id_; }
    void setId(long long id) { id_.set(id); }
    void clearId() { id_.clear(); }

    const std::string &name() const { return name_; }
    void setName(const std::string &name) { name_ = name; }

    const std::string &email() const { return email_; }
    void setEmail(const std::string &email) { email_ = email; }

    const Nullable<std::string> &address() const { return address_; }
    void setAddress(const std::string &address) { address_.set(address); }
    void clearAddress() { address_.clear(); }

    // Serialises only the properties that actually hold a value. A person with
    // no address has no "address" key at all, rather than an explicit null.
    picojson::value toJson() const;
    std::string toJsonString() const;

    // Reads a person from a JSON object. An absent or null "address" leaves the
    // field null; an absent "id" leaves the person unsaved. Returns false and
    // fills `error` when the payload is not a JSON object or a field has the
    // wrong type.
    static bool fromJson(const picojson::value &json, Person &out, std::string &error);

    // Checks the constraints the table enforces, so a bad payload is rejected
    // with a readable message instead of a database error. UNIQUE(email) is not
    // checked here -- only the database can answer that.
    bool validate(std::string &error) const;

private:
    Nullable<long long> id_;
    std::string name_;
    std::string email_;
    Nullable<std::string> address_;
};

} // namespace models

#endif // __MODELS_PERSON_H__
