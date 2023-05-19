# Search Server

SearchServer - это система, написанная на С++ и предназначенная для поиска документов по ключевым словам.

## Основные функции:
* ранжирование результатов поиска по статистической мере TF-IDF
* обработка стоп-слов и минус-слов
* работа в многопоточном режиме
* добавление и удаление документов
* создание и обработка очереди запросов
* постраничное разделение результатов поиска

## Использование поискового сервера
### Создание экземпляра класса SearchServer. 
В конструктор передаётся либо строка с стоп-словами, разделенными пробелами, либо произвольный контейнер, у которого есть возможность использования в for-range цикла.
```cpp
// передаем строку
SearchServer search_server_first("and with on or"s);

// передаем контейнеры
std::vector<std::string> stop_words_vector{"and"s, "with"s, "on"s, "or"s};
std::set<std::string> stop_words_set{"and"s, "with"s, "on"s, "or"s};

SearchServer search_server_second(stop_words_vector);
SearchServer search_server_third(stop_words_set);
```

### Добавление документов
Метод `AddDocument` принимет id документа, статус, рейтинг и сам документ в формате строки.

Статус документа может быть:
* ACTUAL - обозначает, что документ актуален.
* IRRELEVANT - обозначает, что документ неактуален или его не существует.
* BANNED - обозначает, что документ заблокирован.
* REMOVED - обозначает, что документ удален.
```cpp
search_server.AddDocument(123, "This is a simple document"s, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
```

### Удаление документов
Метод `RemoveDocument` удаляет документ по его id. Метод реализован в однопоточной и в многопоточной версиях.
```cpp
auto policy = std::execution::seq или std::execution::par;
search_server.RemoveDocument(123);
search_server.RemoveDocument(policy, 123);
```

### Удаление дубликатов
Функция `RemoveDuplicates` удаляет дублирующиеся документы из поискового сервера. Она принимает ссылку на объект типа `SearchServer` в качестве параметра.
```cpp
RemoveDuplicates(search_server);
```

### Поиск документов
Метод `FindTopDocuments` возвращает вектор документов по ключевым словам запроса. Результаты отсортированы по статистической мере TF-IDF. Возможно добавить фильтрацию документов по id, статусу и рейтингу. Метод реализован в однопоточной и в многопоточной версиях.
```cpp
result = search_server.FindTopDocuments("What is a search server?");
result = search_server.FindTopDocuments("What is a search server?", DocumentStatus::ACTUAL);
result = search_server.FindTopDocuments(std::execution::seq, "What is a search server?");
result = search_server.FindTopDocuments(std::execution::par, "What is a search server?");
```

### Количество документов
Метод `GetDocumentCount` возвращает количество документов.
```cpp
int count = search_server.GetDocumentCount();
```

### Итерация по документам
Методы `begin` и `end` предназначены для того, чтобы можно было итерироваться по search server. Итератор даёт доступ к id всех документов, хранящихся в поисковом сервере. 
```cpp
for (auto it = search_server.begin(); it != search_server.end(); it++) {
    // pass
}
```

### Частота слов в документе
Метод `GetWordFrequencies` возвращает частоты слов в документе с заданным идентификатором.
```cpp
std::map<std::string_view, double> words_freq = search_server.GetWordFrequencies(123);
```

### Проверка документа запросу
Метод `MatchDocument` проверяет, подходит ли документ под критерии поискового запроса. Метод реализован в однопоточной и в многопоточной версиях.
```cpp
auto policy = std::execution::seq или std::execution::par;
result = search_server.MatchDocument("What is a search server?", 123);
result = search_server.MatchDocument(policy, "What is a search server?", 123);
```

### Класс Paginator
Класс `Paginator` обеспечивает отображение документов постранично.
```cpp
// создаём экземпляр поискового сервера со списком стоп слов
std::vector<std::string> stop_words{"and"s, "with"s, "on"s, "or"s};
SearchServer search_server(stop_words);

// добавляем документы в сервер
search_server.AddDocument(1, "First document"s, DocumentStatus::ACTUAL, {1, 2});
search_server.AddDocument(2, "Second document"s, DocumentStatus::ACTUAL, {1, 2});
search_server.AddDocument(3, "Third document"s, DocumentStatus::ACTUAL, {1, 2});
search_server.AddDocument(4, "Fourth document"s, DocumentStatus::ACTUAL, {1, 2});
search_server.AddDocument(5, "Fifth document"s, DocumentStatus::ACTUAL, {1, 2});

const auto search_results = search_server.FindTopDocuments("document");

// показываем результат поиска по страницам
size_t page_size = 2;
const auto pages = Paginate(search_results, page_size);
for (const auto &page: pages) {
    std::cout << page << std::endl;
    std::cout << "Page break"s << std::endl;
}
```

### Класс RequestQueue
Класс `RequestQueue` обеспечивает хранение истории запросов к поисковому серверу, при этом общее количество хранящихся запросов ограничено заданным значением. При добавлении новых запросов, самые старые запросы в очереди замещаются.
```cpp
SearchServer search_server("and in at"s);
RequestQueue request_queue(search_server);
search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
// 1439 запросов с нулевым результатом
for (int i = 0; i < 1439; ++i) {
    request_queue.AddFindRequest("empty request"s);
}
// все еще 1439 запросов с нулевым результатом
request_queue.AddFindRequest("curly dog"s);
// новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
request_queue.AddFindRequest("big collar"s);
// первый запрос удален, 1437 запросов с нулевым результатом
request_queue.AddFindRequest("sparrow"s);
std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
```

### Функции ProcessQueries и ProcessQueriesJoined
Функция `ProcessQueries` распараллеливает обработку нескольких запросов к поисковой системе.

Функцию `ProcessQueriesJoined` подобно функции `ProcessQueries` распараллеливает обработку нескольких запросов к поисковой системе, но возвращает набор документов в плоском виде.
```cpp
SearchServer search_server("and with"s);
int id = 0;

for (const string &text: {
        "funny pet and nasty rat"s,
        "funny pet with curly hair"s,
        "funny pet and not very nasty rat"s,
        "pet with rat and rat and rat"s,
        "nasty rat with curly hair"s,
}
        ) {
    search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
}

const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
};

for (const Document &document: ProcessQueriesJoined(search_server, queries)) {
    cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
}
```

## Класс LogDuration
Заголовочный файл `log_duration.h` содержит определение класса `LogDuration` и два макроса: `LOG_DURATION` и `LOG_DURATION_STREAM`. Класс `LogDuration` используется для измерения затраченного времени выполнения определенного блока кода. Это может быть полезно для профилирования производительности и определения узких мест в программе.

1. `LogDuration` класс имеет два публичных метода:
    * Конструктор `LogDuration(std::string_view id, std::ostream& dst_stream = std::cerr)`: принимает идентификатор блока кода в виде строки `(id)` и необязательный параметр `dst_stream`, который определяет поток, в который будет выведено измеренное время (по умолчанию используется `std::cerr`).

    * Деструктор `~LogDuration()` вызывается при выходе из блока кода, и в нем происходит измерение времени выполнения блока. Разница между текущим временем и временем начала измерений вычисляется с помощью `std::chrono::steady_clock`. Результат измерений выводится в поток `dst_stream` в формате "идентификатор: время_в_миллисекундах ms", где `идентификатор` - это переданная строка `id`, а `время_в_миллисекундах` - это измеренное время в миллисекундах.


2. Макрос `LOG_DURATION`:
`LOG_DURATION(x)`: замеряет время, прошедшее с момента своего вызова до конца текущего блока кода. Измеренное время выводится в поток `std::cerr`. `x` - это строковое значение, которое будет использовано для идентификации измеренного времени.
Пример использования макроса `LOG_DURATION`:
    ```cpp
    void Task1() {
        LOG_DURATION("Task 1"s); // Выведет в cerr время работы функции Task1
        // ...
    }

    void Task2() {
        LOG_DURATION("Task 2"s); // Выведет в cerr время работы функции Task2
        // ...
    }

    int main() {
        LOG_DURATION("main"s);  // Выведет в cerr время работы функции main
        Task1();
        Task2();
    }
    ```

3. Макрос `LOG_DURATION_STREAM`:
`LOG_DURATION_STREAM(x, y)`: аналогичен макросу `LOG_DURATION`, но позволяет указать поток `(y)`, в который будет выведено измеренное время.
Пример использования макроса `LOG_DURATION_STREAM`:
    ```cpp
    int main() {
        // Выведет время работы main в поток std::cout
        LOG_DURATION_STREAM("main"s, std::cout);
        // ...
    }
    ```
    ```python
    В результате использования макросов будет выведено, например:  TestSearchServer: 123 ms
    ```

## Системные требования
* Компилятор С++ с поддержкой стандарта C++17 или новее.
* Для сборки многопоточных версий методов может понабодится `Intel TBB`.
* Если у вас установлена macOS, то компилятор `clang`, скорее всего, не поддерживает библиотеку `execution`. В таком случае можно использовать компилятор `gcc`.