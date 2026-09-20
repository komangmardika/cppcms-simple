#ifndef __PERSON_SERVICE_H__
#define __PERSON_SERVICE_H__

#include <ostream>
#include <string>
#include <vector>
#include <cppdb/frontend.h>
#include <models/Person.h>

namespace services {

// All persistence for the `person` table lives here, so the controller only
// deals with HTTP and JSON and the tests can exercise CRUD without cppcms.
class PersonService
{
public:
    // Why a write succeeded or failed. Callers decide how to report each case;
    // the service deliberately does not know about HTTP status codes, and
    // callers never have to match on the text of `error` to tell them apart.
    enum Status {
        Success,
        NotFound,
        ValidationFailed,
        DuplicateEmail,
        DatabaseError
    };

    static const char *statusName(Status status);

    // The session is borrowed, not owned -- it outlives the service.
    explicit PersonService(cppdb::session &sql);

    // The DDL of the `person` table. The table already exists in db.db; this is
    // here so tests can build the same shape in a throwaway database.
    static const char *schemaSql();

    // Inserts the person and stamps it with the id the database assigned.
    Status create(models::Person &person, std::string &error);

    // Updates every column of an existing row. The person must carry an id.
    Status update(const models::Person &person, std::string &error);

    // Returns NotFound when no row has that id.
    Status remove(long long id);

    // Reads report absence with a bool -- they have only the two outcomes.
    bool findById(long long id, models::Person &out);
    std::vector<models::Person> findAll();

    bool existsByEmail(const std::string &email);
    long long count();

private:
    cppdb::session &sql_;

    // Reads the current row of `res` into a Person, leaving address null when
    // the column is NULL.
    static models::Person mapRow(cppdb::result &res);

    // Binds the address placeholder, sending SQL NULL when there is no address.
    static void bindAddress(cppdb::statement &st, const models::Person &person);

    // SELECT of every column, with an optional trailing clause.
    static std::string selectSql(const std::string &clause);
};

// Lets assertions and logs print a readable name rather than an integer.
inline std::ostream &operator<<(std::ostream &out, PersonService::Status status)
{
    return out << PersonService::statusName(status);
}

} // namespace services

#endif // __PERSON_SERVICE_H__
