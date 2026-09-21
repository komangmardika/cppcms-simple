#include "TestFramework.h"

#include <cstdio>
#include <sstream>
#include <string>

#include <models/Person.h>
#include <services/PersonService.h>

using models::Person;
using services::PersonService;

namespace {

// Each test gets its own throwaway sqlite file, so no test can see rows left
// behind by another and the suite is order-independent.
class TempDatabase
{
public:
    TempDatabase()
    {
        static int counter = 0;
        std::ostringstream path;
        path << "person_tests_" << ++counter << ".sqlite3";
        path_ = path.str();

        std::remove(path_.c_str());
        sql_.open("sqlite3:db=" + path_);

        cppdb::statement schema = sql_ << PersonService::schemaSql();
        schema.exec();
    }

    ~TempDatabase()
    {
        sql_.close();
        std::remove(path_.c_str());
    }

    cppdb::session &sql() { return sql_; }

private:
    std::string path_;
    cppdb::session sql_;
};

// Builds a valid person, so each test only spells out the field it cares about.
Person samplePerson(const std::string &name = "Ada Lovelace",
                    const std::string &email = "ada@example.com")
{
    return Person(name, email);
}

// Re-parses serialised JSON so assertions talk about structure rather than
// depending on key ordering.
picojson::object parseObject(const std::string &json)
{
    picojson::value parsed;
    const std::string error = picojson::parse(parsed, json);
    if (!error.empty())
        TEST_FAIL("expected valid JSON, got parse error: " << error);
    if (!parsed.is<picojson::object>())
        TEST_FAIL("expected a JSON object, got: " << json);
    return parsed.get<picojson::object>();
}

bool hasKey(const picojson::object &obj, const std::string &key)
{
    return obj.find(key) != obj.end();
}

} // namespace

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

TEST(create_assigns_an_autoincrement_id)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson();
    CHECK_FALSE(person.id().hasValue());

    std::string error;
    CHECK_EQ(service.create(person, error), PersonService::Success);
    CHECK_EQ(error, std::string(""));
    CHECK(person.id().hasValue());
    CHECK(person.id().value() > 0);
}

TEST(create_increments_the_id_for_each_row)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person first = samplePerson("Ada", "ada@example.com");
    Person second = samplePerson("Grace", "grace@example.com");

    CHECK_EQ(service.create(first, error), PersonService::Success);
    CHECK_EQ(service.create(second, error), PersonService::Success);
    CHECK_EQ(second.id().value(), first.id().value() + 1);
}

TEST(create_persists_the_address_when_given)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson();
    person.setAddress("12 Analytical Engine Way");

    std::string error;
    CHECK_EQ(service.create(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK(loaded.address().hasValue());
    CHECK_EQ(loaded.address().value(), std::string("12 Analytical Engine Way"));
}

TEST(create_leaves_the_address_null_when_omitted)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson();
    std::string error;
    CHECK_EQ(service.create(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK_FALSE(loaded.address().hasValue());
}

TEST(create_rejects_a_duplicate_email)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person first = samplePerson("Ada", "ada@example.com");
    CHECK_EQ(service.create(first, error), PersonService::Success);

    Person duplicate = samplePerson("Someone Else", "ada@example.com");
    CHECK_EQ(service.create(duplicate, error), PersonService::DuplicateEmail);
    CHECK(error.find("already exists") != std::string::npos);
    CHECK_EQ(service.count(), 1LL);
}

TEST(create_rejects_a_missing_name)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person("", "ada@example.com");
    std::string error;

    CHECK_EQ(service.create(person, error), PersonService::ValidationFailed);
    CHECK(error.find("'name' is required") != std::string::npos);
    CHECK_EQ(service.count(), 0LL);
}

TEST(create_rejects_a_missing_email)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person("Ada", "");
    std::string error;

    CHECK_EQ(service.create(person, error), PersonService::ValidationFailed);
    CHECK(error.find("'email' is required") != std::string::npos);
}

TEST(create_rejects_a_malformed_email)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person("Ada", "not-an-email");
    std::string error;

    CHECK_EQ(service.create(person, error), PersonService::ValidationFailed);
    CHECK(error.find("not a valid email") != std::string::npos);
}

TEST(create_rejects_values_longer_than_the_columns)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person longName(std::string(101, 'a'), "ada@example.com");
    CHECK_EQ(service.create(longName, error), PersonService::ValidationFailed);
    CHECK(error.find("at most 100") != std::string::npos);

    Person longAddress = samplePerson();
    longAddress.setAddress(std::string(256, 'a'));
    CHECK_EQ(service.create(longAddress, error), PersonService::ValidationFailed);
    CHECK(error.find("at most 255") != std::string::npos);

    // A name of exactly the column width is still acceptable.
    Person atLimit(std::string(100, 'a'), "limit@example.com");
    CHECK_EQ(service.create(atLimit, error), PersonService::Success);
}

// ---------------------------------------------------------------------------
// Read
// ---------------------------------------------------------------------------

TEST(find_by_id_returns_the_stored_fields)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson("Grace Hopper", "grace@example.com");
    person.setAddress("Arlington");
    std::string error;
    CHECK_EQ(service.create(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK_EQ(loaded.id().value(), person.id().value());
    CHECK_EQ(loaded.name(), std::string("Grace Hopper"));
    CHECK_EQ(loaded.email(), std::string("grace@example.com"));
    CHECK_EQ(loaded.address().value(), std::string("Arlington"));
}

TEST(find_by_id_reports_a_missing_row)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person loaded;
    CHECK_FALSE(service.findById(4242, loaded));
}

TEST(find_all_returns_every_row_in_id_order)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person first = samplePerson("Ada", "ada@example.com");
    Person second = samplePerson("Grace", "grace@example.com");
    Person third = samplePerson("Katherine", "katherine@example.com");
    CHECK_EQ(service.create(first, error), PersonService::Success);
    CHECK_EQ(service.create(second, error), PersonService::Success);
    CHECK_EQ(service.create(third, error), PersonService::Success);

    const std::vector<Person> people = service.findAll();
    CHECK_EQ(people.size(), static_cast<size_t>(3));
    CHECK_EQ(people[0].name(), std::string("Ada"));
    CHECK_EQ(people[1].name(), std::string("Grace"));
    CHECK_EQ(people[2].name(), std::string("Katherine"));
}

TEST(find_all_returns_nothing_for_an_empty_table)
{
    TempDatabase db;
    PersonService service(db.sql());

    CHECK_EQ(service.findAll().size(), static_cast<size_t>(0));
    CHECK_EQ(service.count(), 0LL);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

TEST(update_changes_the_stored_fields)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    CHECK_EQ(service.create(person, error), PersonService::Success);

    person.setName("Ada Byron");
    person.setEmail("ada.byron@example.com");
    CHECK_EQ(service.update(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK_EQ(loaded.name(), std::string("Ada Byron"));
    CHECK_EQ(loaded.email(), std::string("ada.byron@example.com"));
}

TEST(update_can_set_an_address_that_was_null)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    CHECK_EQ(service.create(person, error), PersonService::Success);
    CHECK_FALSE(person.address().hasValue());

    person.setAddress("New Address");
    CHECK_EQ(service.update(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK(loaded.address().hasValue());
    CHECK_EQ(loaded.address().value(), std::string("New Address"));
}

TEST(update_can_clear_an_address_back_to_null)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    person.setAddress("Somewhere");
    CHECK_EQ(service.create(person, error), PersonService::Success);

    person.clearAddress();
    CHECK_EQ(service.update(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK_FALSE(loaded.address().hasValue());
}

TEST(update_rejects_an_email_taken_by_another_row)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person first = samplePerson("Ada", "ada@example.com");
    Person second = samplePerson("Grace", "grace@example.com");
    CHECK_EQ(service.create(first, error), PersonService::Success);
    CHECK_EQ(service.create(second, error), PersonService::Success);

    second.setEmail("ada@example.com");
    CHECK_EQ(service.update(second, error), PersonService::DuplicateEmail);
    CHECK(error.find("already exists") != std::string::npos);

    // The stored row is untouched.
    Person loaded;
    CHECK(service.findById(second.id().value(), loaded));
    CHECK_EQ(loaded.email(), std::string("grace@example.com"));
}

TEST(update_allows_a_row_to_keep_its_own_email)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    CHECK_EQ(service.create(person, error), PersonService::Success);

    person.setName("Ada L.");
    CHECK_EQ(service.update(person, error), PersonService::Success);
    CHECK_EQ(error, std::string(""));
}

TEST(update_reports_a_missing_row)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson();
    person.setId(4242);

    std::string error;
    CHECK_EQ(service.update(person, error), PersonService::NotFound);
    CHECK_EQ(error, std::string("no person with that id"));
}

TEST(update_requires_an_id)
{
    TempDatabase db;
    PersonService service(db.sql());

    Person person = samplePerson();
    std::string error;

    CHECK_EQ(service.update(person, error), PersonService::ValidationFailed);
    CHECK(error.find("'id' is required") != std::string::npos);
}

TEST(update_rejects_an_invalid_person)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    CHECK_EQ(service.create(person, error), PersonService::Success);

    person.setName("");
    CHECK_EQ(service.update(person, error), PersonService::ValidationFailed);
    CHECK(error.find("'name' is required") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------

TEST(remove_deletes_the_row)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    CHECK_EQ(service.create(person, error), PersonService::Success);
    CHECK_EQ(service.count(), 1LL);

    CHECK_EQ(service.remove(person.id().value()), PersonService::Success);
    CHECK_EQ(service.count(), 0LL);

    Person loaded;
    CHECK_FALSE(service.findById(person.id().value(), loaded));
}

TEST(remove_reports_a_missing_row)
{
    TempDatabase db;
    PersonService service(db.sql());

    CHECK_EQ(service.remove(4242), PersonService::NotFound);
}

TEST(remove_leaves_other_rows_alone)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person first = samplePerson("Ada", "ada@example.com");
    Person second = samplePerson("Grace", "grace@example.com");
    CHECK_EQ(service.create(first, error), PersonService::Success);
    CHECK_EQ(service.create(second, error), PersonService::Success);

    CHECK_EQ(service.remove(first.id().value()), PersonService::Success);
    CHECK_EQ(service.count(), 1LL);

    Person loaded;
    CHECK(service.findById(second.id().value(), loaded));
    CHECK_EQ(loaded.name(), std::string("Grace"));
}

// ---------------------------------------------------------------------------
// JSON: null properties must not appear in responses
// ---------------------------------------------------------------------------

TEST(json_omits_the_address_when_it_is_null)
{
    Person person = samplePerson();
    person.setId(1);

    const std::string json = person.toJsonString();
    const picojson::object obj = parseObject(json);

    CHECK_FALSE(hasKey(obj, "address"));
    CHECK(hasKey(obj, "id"));
    CHECK(hasKey(obj, "name"));
    CHECK(hasKey(obj, "email"));

    // Not even as an explicit null.
    CHECK(json.find("address") == std::string::npos);
    CHECK(json.find("null") == std::string::npos);
}

TEST(json_includes_the_address_when_it_is_set)
{
    Person person = samplePerson();
    person.setId(1);
    person.setAddress("12 Analytical Engine Way");

    const picojson::object obj = parseObject(person.toJsonString());
    CHECK(hasKey(obj, "address"));
    CHECK_EQ(obj.find("address")->second.get<std::string>(),
             std::string("12 Analytical Engine Way"));
}

TEST(json_omits_the_id_for_an_unsaved_person)
{
    const Person person = samplePerson();

    const picojson::object obj = parseObject(person.toJsonString());
    CHECK_FALSE(hasKey(obj, "id"));
    CHECK(hasKey(obj, "name"));
}

TEST(json_renders_the_id_as_a_plain_integer)
{
    Person person = samplePerson();
    person.setId(42);

    const std::string json = person.toJsonString();
    CHECK(json.find("\"id\":42") != std::string::npos);
}

TEST(json_keeps_an_empty_address_distinct_from_a_missing_one)
{
    Person person = samplePerson();
    person.setAddress("");

    const picojson::object obj = parseObject(person.toJsonString());
    CHECK(hasKey(obj, "address"));
    CHECK_EQ(obj.find("address")->second.get<std::string>(), std::string(""));
}

// ---------------------------------------------------------------------------
// Partial updates: applyJson merges onto an existing person
// ---------------------------------------------------------------------------

TEST(apply_json_leaves_absent_fields_unchanged)
{
    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");
    person.setId(7);

    picojson::value payload;
    picojson::parse(payload, "{\"name\":\"Ada Byron\"}");

    std::string error;
    CHECK(Person::applyJson(payload, person, error));
    CHECK_EQ(person.name(), std::string("Ada Byron"));
    CHECK_EQ(person.email(), std::string("ada@example.com"));
    CHECK_EQ(person.address().value(), std::string("Original Street"));
    CHECK_EQ(person.id().value(), 7LL);
}

TEST(apply_json_can_update_only_the_address)
{
    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");

    picojson::value payload;
    picojson::parse(payload, "{\"address\":\"New Street\"}");

    std::string error;
    CHECK(Person::applyJson(payload, person, error));
    CHECK_EQ(person.address().value(), std::string("New Street"));
    CHECK_EQ(person.name(), std::string("Ada"));
}

TEST(apply_json_clears_the_address_on_an_explicit_null)
{
    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");

    picojson::value payload;
    picojson::parse(payload, "{\"address\":null}");

    std::string error;
    CHECK(Person::applyJson(payload, person, error));
    CHECK_FALSE(person.address().hasValue());
    CHECK_EQ(person.name(), std::string("Ada"));
}

TEST(apply_json_with_an_empty_object_changes_nothing)
{
    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");

    picojson::value payload;
    picojson::parse(payload, "{}");

    std::string error;
    CHECK(Person::applyJson(payload, person, error));
    CHECK_EQ(person.name(), std::string("Ada"));
    CHECK_EQ(person.email(), std::string("ada@example.com"));
    CHECK_EQ(person.address().value(), std::string("Original Street"));
}

TEST(apply_json_leaves_the_person_untouched_on_a_type_error)
{
    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");

    picojson::value payload;
    picojson::parse(payload, "{\"name\":\"Valid\",\"address\":123}");

    std::string error;
    CHECK_FALSE(Person::applyJson(payload, person, error));
    CHECK(error.find("'address' must be a string") != std::string::npos);

    // The earlier valid "name" must not have been applied.
    CHECK_EQ(person.name(), std::string("Ada"));
    CHECK_EQ(person.address().value(), std::string("Original Street"));
}

TEST(partial_update_round_trips_through_the_database)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person("Ada", "ada@example.com");
    person.setAddress("Original Street");
    CHECK_EQ(service.create(person, error), PersonService::Success);

    // The payload a client would send to change only the address.
    Person stored;
    CHECK(service.findById(person.id().value(), stored));

    picojson::value payload;
    picojson::parse(payload, "{\"address\":\"New Street\"}");
    CHECK(Person::applyJson(payload, stored, error));
    CHECK_EQ(service.update(stored, error), PersonService::Success);

    Person reloaded;
    CHECK(service.findById(person.id().value(), reloaded));
    CHECK_EQ(reloaded.address().value(), std::string("New Street"));
    CHECK_EQ(reloaded.name(), std::string("Ada"));
    CHECK_EQ(reloaded.email(), std::string("ada@example.com"));
}

TEST(from_json_reads_every_field)
{
    picojson::value parsed;
    picojson::parse(parsed,
                    "{\"id\":7,\"name\":\"Ada\",\"email\":\"ada@example.com\","
                    "\"address\":\"Somewhere\"}");

    Person person;
    std::string error;
    CHECK(Person::fromJson(parsed, person, error));
    CHECK_EQ(person.id().value(), 7LL);
    CHECK_EQ(person.name(), std::string("Ada"));
    CHECK_EQ(person.email(), std::string("ada@example.com"));
    CHECK_EQ(person.address().value(), std::string("Somewhere"));
}

TEST(from_json_treats_an_explicit_null_address_as_absent)
{
    picojson::value parsed;
    picojson::parse(parsed, "{\"name\":\"Ada\",\"email\":\"ada@example.com\",\"address\":null}");

    Person person;
    std::string error;
    CHECK(Person::fromJson(parsed, person, error));
    CHECK_FALSE(person.address().hasValue());
}

TEST(from_json_rejects_a_non_object_payload)
{
    picojson::value parsed;
    picojson::parse(parsed, "[1,2,3]");

    Person person;
    std::string error;
    CHECK_FALSE(Person::fromJson(parsed, person, error));
    CHECK(error.find("must be a JSON object") != std::string::npos);
}

TEST(from_json_rejects_a_field_of_the_wrong_type)
{
    picojson::value parsed;
    picojson::parse(parsed, "{\"name\":123,\"email\":\"ada@example.com\"}");

    Person person;
    std::string error;
    CHECK_FALSE(Person::fromJson(parsed, person, error));
    CHECK(error.find("'name' must be a string") != std::string::npos);
}

TEST(json_survives_a_round_trip_through_the_database)
{
    TempDatabase db;
    PersonService service(db.sql());
    std::string error;

    Person person = samplePerson();
    person.setAddress("Round Trip Lane");
    CHECK_EQ(service.create(person, error), PersonService::Success);

    Person loaded;
    CHECK(service.findById(person.id().value(), loaded));
    CHECK_EQ(loaded.toJsonString(), person.toJsonString());
}

int main()
{
    std::cout << "running person tests" << std::endl;
    return testing::run();
}
