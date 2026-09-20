
#include <cppcms/application.h>  
#include <cppcms/applications_pool.h>  
#include <cppcms/service.h>  
#include <cppcms/http_response.h>  
#include <cppcms/http_request.h>  
#include <cppcms/url_mapper.h>
#include <iostream> 
#include <map>
#include <controllers/User.h>
#include <controllers/Auth.h>
#include <controllers/Person.h>

class hello : public cppcms::application {  
public:
    hello(cppcms::service &srv) :  
        cppcms::application(srv)  
    {
        attach(new User(srv), "users", "/users/{1}",
            "/users(/(.*))?", 1
        );
        attach(new Auth(srv), "auth", "/auth/{1}",
            "/auth(/(.*))?", 1
        );
        attach(new Person(srv), "persons", "/persons/{1}",
            "/persons(/(.*))?", 1
        );
        mapper().root("/");
    }
};

int main(int argc,char ** argv)  
{  
    try {  
        cppcms::service srv(argc,argv);  
        srv.applications_pool().mount(  
            cppcms::applications_factory<hello>()  
        );  
        srv.run();  
    }  
    catch(std::exception const &e) {  
        std::cerr << e.what() << std::endl;  
    }  
}
