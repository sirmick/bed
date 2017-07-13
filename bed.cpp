#define CROW_ENABLE_SSL
#include "crow_all.h"

#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <map>
//#include <mkdev.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <openssl/rand.h>

#define KB 1024

using namespace std;
using namespace crow;
using namespace boost::filesystem;
using namespace boost::algorithm;
using namespace boost::program_options;


template<typename T> struct map_init_helper
{
  T& data;
  map_init_helper(T& d) : data(d) {}
  map_init_helper& operator() (typename T::key_type const& key, typename T::mapped_type const& value)
  {
    data[key] = value;
    return *this;
  }
};

template<typename T> map_init_helper<T> map_init(T& item)
{
  return map_init_helper<T>(item);
}

class EditorLogHandler : public ILogHandler {
  public:
      void log(string /*message*/, LogLevel /*level*/) override {

      }
};

struct EditorStatic
{
  path root;
  string password;
  map<const string, string> mime_types;
  EditorStatic() : root()
  {
    map_init(mime_types)
      (".js", "application/javascript")
      (".html", "text/html")
      (".css", "text/css")
    ;
  }

  void init(path root, string password){
    this->root = root;
    this->password = password;
  }

  struct context
  {
  };

  void before_handle(request& req, response& res, context& /*ctx*/)
  {
    if(contains(req.url,"?")) return;
    if(contains(req.url,"..")) return;
    if(req.url == "/") return send_file("index.html", res);
    if(req.url == "/index.css") return send_file("index.css", res);
    if(starts_with(req.url, "/node_modules")) return send_file(req.url, res);
  }

  void send_file(string file_path, response& res){
    path abs_path = root / file_path;
    string ext = extension(abs_path);
    to_lower(ext);
    auto mime_type = mime_types.find(ext);
    if(mime_type == mime_types.end()) res.add_header("Content-Type", "text/plain");
    else res.add_header("Content-Type", mime_type->second);
    if (!is_regular_file(abs_path)) {
      res.code = 404;
      res.write("File not found");
      res.end();
    }
    stringstream buffer;
    std::ifstream file(abs_path.string());
    buffer << file.rdbuf();
    res.write(buffer.str());
    res.end();
  }

  void after_handle(request& /*req*/, response& /*res*/, context& /*ctx*/)
  {
      // no-op
  }
};

tuple<string, uid_t, gid_t> get_owner(path p){
  struct stat info;
  int r = stat(p.c_str(), &info);
  if(r < 0){
    return tuple<string, uid_t, gid_t>(strerror(errno), 0, 0);
  }
  struct passwd *pw = getpwuid(info.st_uid);
  return tuple<string, uid_t, gid_t>(pw->pw_name, pw->pw_uid, pw->pw_gid);
}

string generate_password(int length){
  unsigned char *random = (unsigned char *)malloc(length);
  int rc = RAND_bytes(random, length);
  unsigned long err = ERR_get_error();
  if(rc != 1) {
    cout << "Could not generate password" << ERR_error_string(err, NULL);
  }
  const char lookup[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  string password("");
  for(int i = 0; i < length; i++){
    password += lookup[random[i] % sizeof(lookup)];
  }
  free(random);
  return password;
}

/*pair<long, long> get_dev(path p){
  struct stat info;
  int r = stat(p.c_str(), &info);
  if(r < 0){
    return pair<long, long>(0, 0);
  }

  return pair<long, long>(major(info->st_rdev), (long) minor(info->st_rdev));
}*/

tuple<string, gid_t> get_group(path p){
  struct stat info;
  int r = stat(p.c_str(), &info);
  if(r < 0){
    return tuple<string, gid_t>(strerror(errno), 0);
  }
  struct group  *gr = getgrgid(info.st_gid);
  return tuple<string, gid_t>(gr->gr_name, gr->gr_gid);
}

int main(int argc, char **argv, char** envp)
{
  path exe_dir(initial_path<path>());
  exe_dir = system_complete(path(argv[0])).parent_path();
  exe_dir = canonical(exe_dir);
  string password = generate_password(10);
  path static_path = exe_dir / "editor-static";
  string to_edit;
  int port;
  string key_file;
  string cert_file;
  bool ssl;
  variables_map arguments;


  options_description keywork_options{"Options"};
  keywork_options.add_options()
    ("help,h", "Help screen")
    ("port", value<int>(&port)->default_value(8081), "The port to listen on")
    ("ssl", value<bool>(&ssl)->default_value(false), "Use HTTPS")
    ("cert", value<string>(&cert_file)->default_value("localhost.crt"), "Cert file to use")
    ("key", value<string>(&key_file)->default_value("localhost.key"), "Key file to use")
    ("to-edit", value<string>(&to_edit)->default_value("."), "File or directory to edit");

  positional_options_description positional_options;
  positional_options.add("to-edit", -1);

  try
  {
    store(command_line_parser(argc, argv).options(keywork_options).positional(positional_options).run(), arguments);
    notify(arguments);
  }
  catch (const error &ex)
  {
    std::cerr << ex.what() << '\n';
    exit(1);
  }

  if (arguments.count("help")) {
    std::cout
      << "Bed (Browser EDitor) is a browser based text editor, you start bed on your remote machine pointing to a directory or file you wish to edit and then use your browser to connect and edit\n"
      << "Usage: bed <options> to-edit\n"
      << keywork_options
      << endl;
    exit(0);
  }

  crow::App<EditorStatic> app;
  app.get_middleware<EditorStatic>().init(static_path, password);

  // Get hostname
  char _hostname[1024];
  gethostname(_hostname, 1024);
  string hostname(_hostname);

  // Get arguments
  vector<string> args;
  char **arg;
  for (arg = argv; *arg != 0; arg++) args.push_back(*arg);

  // Get environment
  vector<string> env;
  char **var;
  for (var = envp; *var != 0; var++) env.push_back(*var);

  path initial_dir;
  path initial_file;
  path initial = canonical(path(to_edit));
  if(is_directory(initial)) initial_dir = initial;
  else if(is_regular_file(initial)){
    initial_dir = initial.parent_path();
    initial_file = initial;
  }


  map<const file_type, string> file_types;
  map_init(file_types)
    (status_error, "status error")
    (file_not_found, "not found")
    (regular_file, "file")
    (directory_file, "directory")
    (symlink_file, "symlink")
    (block_file, "block")
    (character_file, "char")
    (fifo_file, "fifo")
    (socket_file, "socket")
    (type_unknown, "unkown")
  ;

  CROW_ROUTE(app, "/read")
  ([](const request& req){
    string q = req.url_params.get("p");
    path p(q);
    if (!exists(p)) return response(404, "404 File not found");
    if (!is_regular_file(p)) return response(405, "405 Not a file");
    if(file_size(p) > 100*KB) return response(412, "412 Too large");
    stringstream buffer;
    std::ifstream file(p.string());
    if (!file.good()) return response(403, "403 Can't read");
    buffer << file.rdbuf();
    return response(200, buffer.str());
  });

  CROW_ROUTE(app, "/write").methods("POST"_method)
  ([](const request& req){
    string q = req.url_params.get("p");
    path p(q);
    std::ofstream file(p.string());
    if (!file.good()) return response(403, "403 Can't write");
    file << req.body;
    if (!file.good()) return response(403, "403 Can't write");
    return response(200);
  });


  CROW_ROUTE(app, "/info")
  ([args, env, hostname, initial_file, initial_dir](){{}
    json::wvalue j;
    j["hostname"] = hostname;

    uid_t uid = geteuid ();
    struct passwd *pw = getpwuid (uid);

    j["cwd"] = current_path().string();
    j["user"] = string(pw->pw_name);
    j["initial_file"] = initial_file.string();
    j["initial_dir"] = initial_dir.string();

    int i=0;
    for (string arg : args) j["arg"][i++] = arg;

    i=0;
    for (string var : env) j["env"][i++] = var;

    return response(j);
  });

  CROW_ROUTE(app, "/ls")
  ([file_types](const request& req){
    string q = req.url_params.get("p");
    path p(q);
    if (!exists(p)) return response(404, "404 Directory not found");
    if (!is_directory(p)) return response(404, "404 Directory not found");
    json::wvalue j;

    auto p_space = space(p);
    auto p_status = status(p);
    const string p_type  = (*(file_types.find(p_status.type()))).second;
    const int p_perms = p_status.permissions();
    auto p_owner = get_owner(p);
    auto p_group = get_group(p);
    j["."]["type"] = p_type;
    j["."]["permissions"] = p_perms;
    j["."]["mtime"] = last_write_time(p);
    j["."]["group"]["name"] = get<0>(p_group);
    j["."]["group"]["gid"] = get<1>(p_group);
    j["."]["owner"]["name"] = get<0>(p_owner);
    j["."]["owner"]["uid"] = get<1>(p_owner);
    j["."]["owner"]["gid"] = get<2>(p_owner);
    j["."]["capacity"] = p_space.capacity;
    j["."]["free"] = p_space.free;
    j["."]["free"] = p_space.available;
    if (is_symlink(p)){
      j["."]["target"] = read_symlink(p).string();
    }

    for (directory_iterator iter(p); iter != directory_iterator(); ++iter){
      directory_entry &entry = *iter; 
    //for (directory_entry& entry : directory_iterator(p)){
      const string pathname = entry.path().string();
      const string type  = (*(file_types.find(entry.status().type()))).second;
      const int perms = entry.status().permissions();
      auto owner = get_owner(entry.path());
      auto group = get_group(entry.path());
      j[pathname]["type"] = type;
      j[pathname]["permissions"] = perms;
      j[pathname]["mtime"] = to_string(last_write_time(entry.path()))   ;
      j[pathname]["group"]["name"] = get<0>(group);
      j[pathname]["group"]["gid"] = get<1>(group);
      j[pathname]["owner"]["name"] = get<0>(owner);
      j[pathname]["owner"]["uid"] = get<1>(owner);
      j[pathname]["owner"]["gid"] = get<2>(owner);
      if(entry.status().type() == regular_file){
        j[pathname]["size"] = to_string(file_size(entry.path()));
      }
      if (is_symlink(p)){
        j[pathname]["target"] = read_symlink(p).string();
      }
      /*if ((entry.status().type() == block_file) || entry.status().type() == character_file))
      {
        auto dev = get_dev(entry.path());
        j[pathname]["major"] = get<0>(dev);
        j[pathname]["major"] = get<1>(dev);
      }*/

    }
    return response(200, j);
  });

  crow::logger::setLogLevel(crow::LogLevel::DEBUG);
  //crow::logger::setHandler(std::make_shared<ExampleLogHandler>());
  cout << "Editing" << to_edit << endl;
  if (!ssl){
    cout << "http://" << hostname << ":" << port << "/?a=" << password << endl;
    app.port(port).multithreaded().run();
  } else {
    cout << "https://" << hostname << ":"  << port << "/?a=" << password << endl;
    app.port(port).multithreaded().ssl_file(cert_file, key_file).run();
  }
}
