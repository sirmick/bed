set -ex
g++ -Wall -std=gnu++11 -pedantic -Wextra -O3 -DNDEBUG -o bed.o -c bed.cpp
g++ -Wall -std=gnu++11 -pedantic -Wextra -O3 -DNDEBUG  bed.o -o bed -rdynamic -lboost_system -lboost_thread -lboost_filesystem -lboost_chrono -lboost_date_time -lboost_atomic -lboost_program_options -lpthread -lssl -lcrypto
