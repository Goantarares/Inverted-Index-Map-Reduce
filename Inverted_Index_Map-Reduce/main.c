#include <bits/stdc++.h>
#include <cctype>
#include <time.h>
using namespace std;

typedef struct thread_struct {
    vector<pair<int, string>> *files; // Lista fișiere, fiecare cu un ID unic
    int thread_id;                   // ID-ul thread-ului curent
    int nr_mappers;                 // Numărul de thread-uri mapper
    int nr_reducers;                // Numărul de thread-uri reducer
    vector<unordered_map<string, set<int>>> *map_results; // Rezultate intermediare mapperi
    pthread_mutex_t *mutex;          // Mutex pentru protejarea accesului
    pthread_barrier_t *barrier;      // Barieră pentru sincronizare
} thread_struct;

// struct timespec start, finish; 
// double elapsed;

std::string strip_word(string& s) {
    std::string stripped_string = "";
    for (char c : s) {
        if (std::isalpha(c)) {
            stripped_string += tolower(c);
        }
    }
    return stripped_string;
}

void bubble_sort(vector<pair<string, set<int>>>& sorted_words) {
    bool swapped;
    int n = sorted_words.size();
    do {
        swapped = false;
        for (int i = 1; i < n; ++i) {
            if (sorted_words[i - 1].second.size() < sorted_words[i].second.size() || 
                (sorted_words[i - 1].second.size() == sorted_words[i].second.size() && sorted_words[i - 1].first > sorted_words[i].first)) {
                swap(sorted_words[i - 1], sorted_words[i]);
                swapped = true;
            }
        }
        --n;
    } while (swapped);
}


void *func(void *thr_att) {
    thread_struct *t = (thread_struct *)thr_att;

    // Mapper
    // clock_gettime(CLOCK_MONOTONIC, &start); 
    if (t->thread_id < t->nr_mappers) {
        pair<int, string> curr_file;

        while (true) {
            pthread_mutex_lock(t->mutex);
            if (t->files->empty()) {
                pthread_mutex_unlock(t->mutex);
                break;
            }
            curr_file = t->files->back();
            t->files->pop_back();
            pthread_mutex_unlock(t->mutex);

            ifstream input(curr_file.second);
            string word;
            while (input >> word) {
                word = strip_word(word);
                if (!word.empty()) {
                    (*t->map_results)[t->thread_id][word].insert(curr_file.first);
                }
            }
        }
    }
    // clock_gettime(CLOCK_MONOTONIC, &finish);
    // elapsed = (finish.tv_sec - start.tv_sec);
    // elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    // cout << "Mapperi : " << elapsed << endl; 

    pthread_barrier_wait(t->barrier);

    // Reducer
    // clock_gettime(CLOCK_MONOTONIC, &start); 
    if (t->thread_id >= t->nr_mappers) {
        unordered_map<string, set<int>> reduce_map;
        int reducer_id = t->thread_id - t->nr_mappers;

        // Distribuire litere intre reduceri
        int letters_per_reducer = 26 / t->nr_reducers;
        char start_letter = 'a' + reducer_id * letters_per_reducer;
        char end_letter = 'a' + (reducer_id + 1) * letters_per_reducer - 1;

        if (reducer_id == t->nr_reducers - 1) {
            end_letter = 'z';
        }

        for (int i = 0; i < t->nr_mappers; i++) {
            for (auto &entry : (*t->map_results)[i]) {
                char first_char = tolower(entry.first[0]);
                if (first_char >= start_letter && first_char <= end_letter) {
                    reduce_map[entry.first].insert(entry.second.begin(), entry.second.end());
                }
            }
        }

        // Sortare cuvinte
        vector<pair<string, set<int>>> sorted_words(reduce_map.begin(), reduce_map.end());
        bubble_sort(sorted_words);

        //Creare toate fisierele
        for (char letter = start_letter; letter <= end_letter; ++letter) {
            string output_filename = string(1, letter) + ".txt";
            ofstream output(output_filename);
            if (!output.is_open()) {
                cerr << "Eroare" << endl;
            }
            output.close();
        }


        for (auto &entry : sorted_words) {
            char first_char = tolower(entry.first[0]);

            if (first_char >= start_letter && first_char <= end_letter) {
                string output_filename =  string(1, first_char) + ".txt";
                ofstream output(output_filename, ios::app);

                if (output.is_open()) {
                    output << entry.first << ":[";

                    bool first = true;
                    for (int file_id : entry.second) {
                        if (!first) {
                            output << " ";
                        }
                        output << file_id;
                        first = false;
                    }

                    output << "]\n";
                    output.close();
                } else {
                    cerr << "Eroare" << endl;
                }
            }
        }
    }
    // clock_gettime(CLOCK_MONOTONIC, &finish);
    // elapsed = (finish.tv_sec - start.tv_sec);
    // elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
    // cout << "Reduceri : " << elapsed << endl; 
    return NULL;
}

int main(int argc, char **argv) {
    int num_files, id, r;

    int M = atoi(argv[1]); // Nr mappers
    int R = atoi(argv[2]); // Nr reducers
    char *file_list = argv[3];

    ifstream input(file_list);
    input >> num_files;

    vector<pair<int, string>> files;
    for (int i = 0; i < num_files; i++) {
        string file_name;
        input >> file_name;
        files.push_back({i + 1, file_name}); //Asociere fisier-ID
    }

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, M + R);

    vector<unordered_map<string, set<int>>> map_results(M);
    vector<thread_struct> thread_attributes(M + R);
    vector<pthread_t> threads(M + R);

    //Creare threaduri
    for (id = 0; id < M + R; id++) {
        thread_attributes[id].files = &files;
        thread_attributes[id].thread_id = id;
        thread_attributes[id].nr_mappers = M;
        thread_attributes[id].nr_reducers = R;
        thread_attributes[id].map_results = &map_results;
        thread_attributes[id].mutex = &mutex;
        thread_attributes[id].barrier = &barrier;

        r = pthread_create(&threads[id], NULL, func, (void *)&thread_attributes[id]);
        if (r) {
            cerr << "Eroare\n";
            exit(-1);
        }
    }

    for (id = 0; id < M + R; id++) {
        r = pthread_join(threads[id], NULL);
        if (r) {
            cerr << "Eroare\n";
            exit(-1);
        }
    }

    pthread_mutex_destroy(&mutex);
    pthread_barrier_destroy(&barrier);

    return 0;
}