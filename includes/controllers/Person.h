#ifndef __PERSON_H__
#define __PERSON_H__

#include <cppcms/application.h>
#include <cppcms/service.h>
#include <data/Master.h>
#include <models/Person.h>
#include <services/PersonService.h>

using database::Master;

// REST endpoints for the `person` table. Everything that touches the database
// lives in services::PersonService, so this class only speaks HTTP and JSON.
//
//   GET    /persons        list every person
//   POST   /persons        create one
//   GET    /persons/{id}   fetch one
//   PUT    /persons/{id}   replace one
//   DELETE /persons/{id}   delete one
class Person : public Master
{
public:
    Person(cppcms::service &srv);

    void getPersons();
    void getPersonById(std::string id);

private:
    void listPersons();
    void createPerson();
    void showPerson(long long id);
    void updatePerson(long long id);
    void deletePerson(long long id);

    bool readPersonFromRequest(models::Person &out, std::string &error);

    // The one place that decides which HTTP status a service outcome deserves.
    static int httpStatusFor(services::PersonService::Status status);
};

#endif
