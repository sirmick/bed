set -ex

c++ -I /usr/local/opt/openssl/include/ -Wall -std=c++1y -pedantic -Wextra -O3 -DNDEBUG -o bed.o -c bed.cpp
c++ -L /usr/local/opt/openssl/lib -Wall -std=c++1y -pedantic -Wextra -O3 -DNDEBUG -Wl,-search_paths_first -Wl,-headerpad_max_install_names bed.o -o bed -lboost_system -lboost_thread-mt -lboost_filesystem-mt -lboost_chrono-mt -lboost_date_time-mt -lboost_atomic-mt -lboost_program_options-mt -lssl -lcrypto
