#ifndef __MASTER_H__
#define __MASTER_H__

#include <string>
#include <cppcms/application.h>

namespace cppdb {
    class session;
}

namespace picojson {
    class value;
}

namespace database {
    class Master: public cppcms::application
    {
    public:
        Master(cppcms::service& srv);
        ~Master();
    protected:
        cppdb::session &sql();

        // Shared JSON response helpers, so every controller gets them for free
        // rather than redefining them.
        void sendJson(int status, const picojson::value &body);
        void sendError(int status, const std::string &message);
        void sendMethodNotAllowed(const std::string &allowed);
    private:
        std::auto_ptr<cppdb::session> sql_;
        std::string conn_str_;
    };
}
#endif
