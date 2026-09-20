#include "PersonService.h"

namespace services {

namespace {

// Every query selects the columns in this order so mapRow() can read them back
// by name regardless of which query produced the result.
const char *PERSON_COLUMNS = "id, name, email, address";

} // namespace

PersonService::PersonService(cppdb::session &sql) : sql_(sql)
{
}

const char *PersonService::statusName(Status status)
{
    switch (status) {
    case Success:          return "Success";
    case NotFound:         return "NotFound";
    case ValidationFailed: return "ValidationFailed";
    case DuplicateEmail:   return "DuplicateEmail";
    case DatabaseError:    return "DatabaseError";
    }
    return "Unknown";
}

const char *PersonService::schemaSql()
{
    return "CREATE TABLE IF NOT EXISTS person ("
           "    id      INTEGER      PRIMARY KEY AUTOINCREMENT,"
           "    name    VARCHAR(100) NOT NULL,"
           "    email   VARCHAR(100) NOT NULL UNIQUE,"
           "    address VARCHAR(255)"
           ")";
}

std::string PersonService::selectSql(const std::string &clause)
{
    return "SELECT " + std::string(PERSON_COLUMNS) + " FROM person " + clause;
}

void PersonService::bindAddress(cppdb::statement &st, const models::Person &person)
{
    if (person.address().hasValue())
        st << person.address().value();
    else
        st << cppdb::null;
}

models::Person PersonService::mapRow(cppdb::result &res)
{
    models::Person person;

    long long id = 0;
    if (res.fetch("id", id))
        person.setId(id);

    std::string name;
    if (res.fetch("name", name))
        person.setName(name);

    std::string email;
    if (res.fetch("email", email))
        person.setEmail(email);

    // fetch() answers false for a NULL column, which is exactly the case where
    // the address should stay absent and drop out of the JSON response.
    std::string address;
    if (res.fetch("address", address))
        person.setAddress(address);

    return person;
}

PersonService::Status PersonService::create(models::Person &person, std::string &error)
{
    if (!person.validate(error))
        return ValidationFailed;

    if (existsByEmail(person.email())) {
        error = "a person with email '" + person.email() + "' already exists";
        return DuplicateEmail;
    }

    try {
        cppdb::statement st =
            sql_ << "INSERT INTO person(name, email, address) VALUES(?, ?, ?)";
        st << person.name() << person.email();
        bindAddress(st, person);

        st.exec();
        person.setId(st.last_insert_id());
    }
    catch (cppdb::cppdb_error const &e) {
        // Covers the UNIQUE(email) race that existsByEmail() cannot close.
        error = std::string("could not create person: ") + e.what();
        return DatabaseError;
    }

    error.clear();
    return Success;
}

PersonService::Status PersonService::update(const models::Person &person, std::string &error)
{
    if (!person.id().hasValue()) {
        error = "'id' is required to update a person";
        return ValidationFailed;
    }
    if (!person.validate(error))
        return ValidationFailed;

    const long long id = person.id().value();

    try {
        // The email must stay unique across every *other* row.
        cppdb::statement taken =
            sql_ << "SELECT id FROM person WHERE email = ? AND id <> ?" << person.email() << id;
        cppdb::result res = taken.query();
        if (res.next()) {
            error = "a person with email '" + person.email() + "' already exists";
            return DuplicateEmail;
        }

        cppdb::statement st =
            sql_ << "UPDATE person SET name = ?, email = ?, address = ? WHERE id = ?";
        st << person.name() << person.email();
        bindAddress(st, person);
        st << id;
        st.exec();

        if (st.affected() == 0) {
            error = "no person with that id";
            return NotFound;
        }
    }
    catch (cppdb::cppdb_error const &e) {
        error = std::string("could not update person: ") + e.what();
        return DatabaseError;
    }

    error.clear();
    return Success;
}

PersonService::Status PersonService::remove(long long id)
{
    cppdb::statement st = sql_ << "DELETE FROM person WHERE id = ?" << id;
    st.exec();
    return st.affected() > 0 ? Success : NotFound;
}

bool PersonService::findById(long long id, models::Person &out)
{
    cppdb::statement st = sql_ << selectSql("WHERE id = ?") << id;
    cppdb::result res = st.query();

    if (!res.next())
        return false;

    out = mapRow(res);
    return true;
}

std::vector<models::Person> PersonService::findAll()
{
    std::vector<models::Person> people;

    cppdb::statement st = sql_ << selectSql("ORDER BY id");
    cppdb::result res = st.query();

    while (res.next())
        people.push_back(mapRow(res));

    return people;
}

bool PersonService::existsByEmail(const std::string &email)
{
    cppdb::statement st = sql_ << "SELECT 1 FROM person WHERE email = ?" << email;
    cppdb::result res = st.query();
    return res.next();
}

long long PersonService::count()
{
    cppdb::statement st = sql_ << "SELECT COUNT(*) FROM person";
    cppdb::result res = st.query();

    if (!res.next())
        return 0;

    long long total = 0;
    res.fetch(0, total);
    return total;
}

} // namespace services
