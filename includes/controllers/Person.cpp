#include "Person.h"
#include <cstdlib>
#include <cppcms/service.h>
#include <cppcms/url_dispatcher.h>
#include <cppcms/url_mapper.h>
#include <cppcms/http_response.h>
#include <cppcms/http_request.h>

using services::PersonService;

// constructor
Person::Person(cppcms::service &srv) : Master(srv)
{
    dispatcher().assign("", &Person::getPersons, this);
    mapper().assign("");

    dispatcher().assign("/(\\d+)", &Person::getPersonById, this, 1);
    mapper().assign("/{1}");
}

int Person::httpStatusFor(PersonService::Status status)
{
    switch (status) {
    case PersonService::Success:          return 200;
    case PersonService::NotFound:         return 404;
    case PersonService::ValidationFailed: return 422;
    // 409 Conflict is arguably the better fit for a uniqueness clash; kept at
    // 422 so the existing API contract does not change.
    case PersonService::DuplicateEmail:   return 422;
    case PersonService::DatabaseError:    return 500;
    }
    return 500;
}

// method for /persons -- GET lists, POST creates
void Person::getPersons()
{
    const std::string req_method = request().request_method();

    if (req_method.compare("GET") == 0)
        listPersons();
    else if (req_method.compare("POST") == 0)
        createPerson();
    else
        sendMethodNotAllowed("GET, POST");
}

// method for /persons/{id} -- GET, PUT and DELETE act on one row
void Person::getPersonById(std::string id)
{
    const long long personId = std::atoll(id.c_str());
    const std::string req_method = request().request_method();

    if (req_method.compare("GET") == 0)
        showPerson(personId);
    else if (req_method.compare("PUT") == 0)
        updatePerson(personId);
    else if (req_method.compare("DELETE") == 0)
        deletePerson(personId);
    else
        sendMethodNotAllowed("GET, PUT, DELETE");
}

void Person::listPersons()
{
    PersonService service(sql());
    const std::vector<models::Person> people = service.findAll();

    picojson::array items;
    items.reserve(people.size());
    for (size_t i = 0; i < people.size(); ++i)
        items.push_back(people[i].toJson());

    sendJson(200, picojson::value(items));
}

void Person::createPerson()
{
    picojson::value payload;
    std::string error;

    if (!readJsonBody(payload, error)) {
        sendError(400, error);
        return;
    }

    models::Person person;
    if (!models::Person::fromJson(payload, person, error)) {
        sendError(400, error);
        return;
    }

    // A client-supplied id is meaningless on create -- the column is
    // autoincrement and the database decides.
    person.clearId();

    PersonService service(sql());
    const PersonService::Status status = service.create(person, error);
    if (status != PersonService::Success) {
        sendError(httpStatusFor(status), error);
        return;
    }

    sendJson(201, person.toJson());
}

void Person::showPerson(long long id)
{
    PersonService service(sql());
    models::Person person;

    if (!service.findById(id, person)) {
        sendError(httpStatusFor(PersonService::NotFound), "no person with that id");
        return;
    }

    sendJson(200, person.toJson());
}

void Person::updatePerson(long long id)
{
    picojson::value payload;
    std::string error;

    if (!readJsonBody(payload, error)) {
        sendError(400, error);
        return;
    }

    // Start from the stored row so the request only has to carry the properties
    // it actually wants to change.
    PersonService service(sql());
    models::Person person;
    if (!service.findById(id, person)) {
        sendError(httpStatusFor(PersonService::NotFound), "no person with that id");
        return;
    }

    if (!models::Person::applyJson(payload, person, error)) {
        sendError(400, error);
        return;
    }

    // The URL is authoritative for which row is being updated.
    person.setId(id);

    const PersonService::Status status = service.update(person, error);
    if (status != PersonService::Success) {
        sendError(httpStatusFor(status), error);
        return;
    }

    sendJson(200, person.toJson());
}

void Person::deletePerson(long long id)
{
    PersonService service(sql());
    const PersonService::Status status = service.remove(id);

    if (status != PersonService::Success) {
        sendError(httpStatusFor(status), "no person with that id");
        return;
    }

    picojson::object body;
    body["deleted"] = picojson::value(static_cast<double>(id));
    sendJson(200, picojson::value(body));
}

bool Person::readJsonBody(picojson::value &out, std::string &error)
{
    std::pair<void *, size_t> post_data = request().raw_post_data();
    const std::string raw(reinterpret_cast<char const *>(post_data.first), post_data.second);

    if (raw.empty()) {
        error = "request body is empty";
        return false;
    }

    const std::string parseError = picojson::parse(out, raw);
    if (!parseError.empty()) {
        error = "invalid JSON: " + parseError;
        return false;
    }

    error.clear();
    return true;
}
