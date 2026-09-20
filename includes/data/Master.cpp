#include "Master.h"
#include <cppdb/frontend.h>
#include <cppcms/json.h>
#include <cppcms/http_response.h>
#include <picojson.h>

namespace database {

Master::Master(cppcms::service &srv) : cppcms::application(srv)
{
    conn_str_ = settings().get<std::string>("cppcms_simple.connection_string");
    sql_.reset(new cppdb::session(conn_str_));
}

Master::~Master()
{
}

cppdb::session &Master::sql()
{
    if (!sql_->is_open())
        sql_->open(conn_str_);
    return *sql_;
}

void Master::sendJson(int status, const picojson::value &body)
{
    response().status(status);
    response().content_type("application/json");
    response().out() << body.serialize();
}

void Master::sendError(int status, const std::string &message)
{
    picojson::object body;
    body["error"] = picojson::value(message);
    sendJson(status, picojson::value(body));
}

void Master::sendMethodNotAllowed(const std::string &allowed)
{
    response().set_header("Allow", allowed);
    sendError(405, "method not allowed, expected one of: " + allowed);
}

}
