---
title: "PSI Projekt 2025Z"
author:

- "Damian D'Souza (lider)"
- "Kamil Marszałek"
- "Michał Szwejk"
date: "11.12.2025"
geometry: margin=2.5cm
documentclass: article
---

## Temat projektu

Program realizujący komunikację w logicznym pierścieniu (token ring) zbudowany w oparciu o protokół UDP.

## Treść

Napisać program realizujący komunikację w logicznym pierścieniu (token ring) w oparciu o protokół UDP.

### Założenia

- W komunikacji uczestniczą wezły / procesy (do testów minimum trzy, ale liczba może być zmienna); węzły identyfikowane są krótkimi nazwami ASCII. Należy mapować adresy IP wezłów na ich logiczne nazwy.
- Komunikacja następuje poprzez pakiet danych, tzw. „token” (znacznik), który jest stale przekazywany cyklicznie między procesami (rysunek 1). Jeśli proces A chce wysłać dane do procesu B, to czeka na otrzymanie pustego tokenu (tokenu bez danych), następnie „doczepia” do niego adres odbiorcy oraz dane przeznaczone do niego i wysyła token to swojego następnika w pierścieniu (rysunki 2, 3, 4, 5 pokazują przekazanie danych z B do D).
- W pierścieniu musi znajdować się zawsze jeden i tylko jeden znacznik.
- Ponieważ korzystamy z protokołu UDP, to w łączności węzeł–węzeł (przekazanie znacznika) powinien być użyty mechanizm gwarantujący niezawodność (może to być znany już protokół BAP lub inny podobny).
- Program powinien mieć charakter modularny. Sugerowane moduły:
  1. „niezawodny” transfer UDP (BAP),
  2. pierścień komunikacyjny (obsługa znacznika, adresacja, ruting, dane),
  3. transport arbitralnych danych w pierścieniu,
  4. testy i cmd-line.
- W testach można założyć, że przekazujemy dane tekstowe.
- Należy wprowadzić opóźnienie między odebraniem a odesłaniem tokenu, sensowna wartość to 1–5 s.
- Interfejs użytkownika – wystarczy prosty interfejs tekstowy. Wskazana jest zarówno realizacja pracy interaktywnej w trybie prostego cmd-line oraz testów realizowanych wsadowo (bez interwencji użytkownika).

### Warianty funkcjonalne

(Każdy zespół otrzyma jeden z wariantów W1 i W2.  
Wariant może modyfikować założenia podane wyżej, wtedy oczywiście ważniejsze są założenia z wariantu.)

- **W11** – Wprowadzić dodatkową funkcję dołączenia procesu do pierścienia. Powinno odbywa się to poprzez broadcast (rozgłaszanie). Węzeł (proces) zgłasza chęć akcesu do komunikacji, zostaje to potwierdzone i w efekcie zmodyfikowana zostaje lokalna tablica rutingu w określonych węzłach (rysunek 1). Realizacja przez dodatkowy „miniprotokół”, możliwych rozwiązań jest wiele, np. dołączenia może dokonać ten proces, który ma aktualnie znacznik. Uwaga – należy tak zaprojektować protokół, aby uniknąć wyścigów i innych niejednoznaczności.

### Warianty implementacyjne  

- **W22** – implementacja w C/C++  

## Interpretacja treści zadania

Celem projektu jest stworzenie aplikacji komunikującej się w logicznym pierścieniu za pomocą protokołu UDP. Aplikacja będzie obsługiwać przekazywanie tokena między procesami, umożliwiając im wysyłanie i odbieranie danych. Dodatkowo, aplikacja będzie implementować mechanizm dołączania nowych procesów do pierścienia za pomocą broadcastu, zapewniając aktualizację tablic routingu i unikając konfliktów.

Funkcje:

- Implementacja niezawodnego transferu UDP za pomocą protokołu BAP.
- Obsługa logiki pierścienia komunikacyjnego, w tym przekazywanie tokena i adresacja.
- Mechanizm dołączania nowych procesów do pierścienia poprzez broadcast.
- Prosty interfejs tekstowy do interakcji z użytkownikiem.

## Opis funkcjonalny (black-box)

System udostępnia następujące funkcje widoczne z zewnątrz:

1. **Uruchomienie węzła**  
   Użytkownik może uruchomić nowy węzeł, podając jego nazwę logiczną oraz podstawową konfigurację (np. port nasłuchu, adres broadcastu). Po uruchomieniu węzeł automatycznie próbuje dołączyć do istniejącego pierścienia albo utworzyć nowy.

2. **Przekazywanie tokena w pierścieniu**  
   Węzły przekazują między sobą token w ustalonej kolejności logicznego pierścienia. W danym momencie dokładnie jeden węzeł posiada token. Token krąży niezależnie od tego, czy aktualnie przesyłane są dane użytkownika.

3. **Wysyłanie wiadomości między węzłami**  
   Użytkownik może zlecić wysłanie wiadomości tekstowej z węzła A do węzła B. Węzeł A czeka na pusty token, dołącza do niego dane (nadawca, odbiorca, treść) i przekazuje token dalej. Węzeł docelowy odbiera dane, a następnie odsyła pusty token.

4. **Dynamiczne dołączanie nowych węzłów (wariant W11)**  
   Nowy węzeł może zgłosić chęć dołączenia poprzez komunikat broadcast. Jeden z istniejących węzłów koordynuje proces dołączenia, tak aby zaktualizować lokalne tablice routingu i wpiąć nowy węzeł w pierścień bez utraty lub duplikacji tokena.

5. **Niezawodny przesył między sąsiadami (BAP)**  
   Wszystkie przekazania tokena i danych między sąsiadującymi węzłami wykorzystują warstwę niezawodnego UDP (protokół BAP lub podobny), zapewniając wykrycie utraty pakietu i retransmisje.

6. **Interfejs użytkownika (CLI)**  
   Użytkownik ma do dyspozycji prosty interfejs tekstowy umożliwiający: uruchomienie węzła, obserwację logów, zlecanie wysyłania wiadomości do innych węzłów oraz (opcjonalnie) uruchamianie scenariuszy testowych w trybie wsadowym.

## Opis i analiza poprawności stosowanych protokołów komunikacyjnych

### Diagram stanu
![Diagram stanu protokołu dołączenia procesu do pieścienia](state_diagram.png)

### Idea na działanie protokołu dołączenia procesu do pieścienia

1. Proces chce dołączyć do pierścienia - wysyła broadcast. Wszystkie procesy w pierścieniu otrzymują broadcast - dodają go do swojej kolejki procesów oczekujących na dołączenie.
2. Proces posiadający token obsługuje dołączenie nowego procesu.
   - Wstrzymuje przepływ tokena do momentu obsłużenia dołączenia wszystkich procesów z kolejki.
   - Wyciągamy proces z kolejki i odsyłamy komunikat typu accept poprzez unicast podając tablicę routingu nowego procesu. Wiadomość otrzymują zainteresowane procesy.
   - Po zakończeniu dołączenia wysyłany jest do wszystkich procesów broadcast typu commit z informacją o nowej wersji pierścienia.
3. Nowy proces czeka na token i zaczyna normalną pracę.

### Pseudokod obsługi dołączenia procesu do pieścienia

```c
int maxfd = max3(broadcast_socket, cli_socket, unicast_socket);
Queue = {}
bool is_sending = false;
while (1) {
    fd_set rfds;
    FD_ZERO(&rfds);

    FD_SET(broadcast_socket, &rfds);
    FD_SET(cli_socket, &rfds);
    FD_SET(unicast_socket, &rfds);

    int ret = select(maxfd + 1, &rfds, NULL, NULL, NULL); 
    if (ret < 0) {
        if (errno == EINTR) continue;  
        perror("select");
        break;
    }
    if (FD_ISSET(unicast_socket, &rfds)) {
        unicast_recv();
    }

    if (FD_ISSET(broadcast_socket, &rfds)) {
        handle_broadcast();
        continue;
    }

    if (Queue.not_empty() && has_token()) {
        handle_join();
    }

    if (FD_ISSET(cli_socket, &rfds) && has_empty_token()) {
        load_data_to_token();
    }

    unicast_send_token_to_successor();
}
```

### Analiza poprawności protokołu dołączenia procesu do pierścienia
Proces A chce dołączyć do pierścienia. Wysyła broadcast, który jest odbierany przez wszystkie procesy w pierścieniu.Proces posiadający token np. proces B sprawdza czy jest w trakcie wysyłania tokena.
- Scenariusz 1: Jeśli tak, to kończy wysyłanie i przekazuje token dalej. Obsługą dołączenia procesu A zajmie się proces będacy następnikiem B. Reszta jak w scenariuszu 2. 

   $\rightarrow$ Przykładowy pierścień na początku: **B[token]** -> C -> D -> B. 

   $\rightarrow$ Po dołączeniu A: B -> A -> **C[token]**-> D -> B
- Scenariusz 2: Proces B posiadający token nie jest w trakcie wysyłania tokena. Odbiera broadcast od procesu A i obsługuje jego dołączenie. Wysyła unicast typu accept do swojego poprzednika oraz do procesu, który chce dołączyć z tablicą routingu. Wysyłanie unicastu nie nastąpi równolegle z przesłaniem tokenu, ponieważ zabezpiecza nas przed tym select. Po dołączeniu procesu wysyłany jest broadcast typu commit, po jego otrzymaniu wszystkie procesy usuwają nowo dołączony proces z kolejki. Proces B staje się następnikiem procesu A w pierścieniu. 

   $\rightarrow$ Przykładowy pierścień na początku: **B[token]** -> C -> D -> B. 

   $\rightarrow$ Po dołączeniu A: A -> **B[token]** -> C-> D -> A

### Opis struktur danych protokołu dołączenia procesu do pieścienia

- Struktura Token:

```c
typedef struct {
    char sender[32];        
    char receiver[32];      
    char data[256];        
    uint8_t is_empty;      
} Token;
```

- Struktura Routing Table Entry:

```c
typedef struct {
    char node_name[32];        
    unsigned short port;   
    unsigned int version;
    Entry* successor;
    Entry* predecessor;     
} Entry;
```
- Enum Type Message:
```c
typedef enum {
    JOIN_REQUEST,
    JOIN_ACCEPT
} MessageType;
```

- Struktura Broadcast Message:

```c
typedef struct {
    MessageType type;      
    char node_name[32]; 
    unsigned short port; 
} BroadcastMessage;
```

### Opis poprawności działania protokołu BAP
Zwykły protokół BAP wydaje się niewystarczający do zapewnienia niezawodnej transmisji w naszym systemie, ponieważ nie uwzględnia on sytuacji, w której ACK może zostać utracony. W takim przypadku nadawca może nie być świadomy, że odbiorca otrzymał wiadomość, co prowadzi do potencjalnych problemów z synchronizacyjnych i wydajnościowych. Dlatego do transmisji unicast zastosujemy zmodyfikowany protokół BAP z dodatkowym potwierdzeniem ACK-ACK.

1. P1 $\rightarrow$ MSG $\rightarrow$ P2         (P1 wysyła do czasu otrzymania ACK)
2. P1 $\leftarrow$ ACK $\leftarrow$ P2           (P2 wysyła do czasu otrzymania ACK-ACK)
3. P1 $\rightarrow$ ACK-ACK $\rightarrow$ P2     (P1 wysyła kilka razy, potem PRZESTAJE po timeout)
4. P2 odbiera ACK-ACK albo po ustalonym timeoucie przestaje wysyłać ACK



Proces obsługujący dołączanie nowego procesu do pierścienia będzie oczekiwał na potwierdzenie od 2 procesów odbioru broadcastu typu ACCEPT (od jego poprzednika i procesu, który chce dołączyć do pierścienia) np. poprzez broadcast typu ACK. W przypadku braku potwierdzenia w określonym czasie proces wysyłający accept będzie ponawiał wysyłanie komunikatu broadcast. Procesy odbierające komunikat broadcast będą wysyłały potwierdzenie odbioru do nadawcy. W ten sposób zostanie zapewniona niezawodność transmisji broadcast.

W przypadku transmisji broadcast typu JOIN_REQUEST nie będzie wymagane potwierdzenie odbioru, ponieważ proces będzie wysyłał co jakiś czas nowe żądanie dołączenie. 
Jeśli w ustalonym czasie nie dostanie ACCEPT to po prostu wyśle ponownie JOIN_REQUEST.

Proces przy wysyłaniu broadcastu JOIN_REQUEST będzie generował identyfikator żądania dołączenia (np. losowa liczba). Procesy odbierające broadcast będą przechowywały identyfikatory już obsłużonych żądań dołączenia, aby uniknąć wielokrotnej obsługi tego samego żądania.

Wersjonowanie - razem z tokenem będzie przesyłana aktualna wersja pierścienia - liczba całkowita zwiększana o 1 przy każdej zmianie w pierścieniu (dołączenie nowego procesu). Procesy przy odbiorze tokena będą porównywały wersję pierścienia w tokenie z lokalną wersją. Jeśli wersja w tokenie będzie nowsza to proces usunie odpowiednią liczbę requestów z kolejki oczekujących do dołączenia (różnica wersji = liczba dołączonych procesów od ostatniego przejścia tokena przez dany proces).

## Planowany podział na moduły i struktura komunikacji

1. **Moduł `reliable_udp` (BAP)** 
   - Wewnątrz modułu znajduje się resolver do mapowania nazw węzłów na adresy IP i porty UDP.
   - Odpowiada za niezawodny transfer między dwoma endpointami UDP.  
   - Udostępnia API w stylu: `rudp_send()`, `rudp_recv()`, które wewnętrznie realizują potwierdzenia, numery sekwencyjne, timeouty i retransmisje.

2. **Moduł `ring_core` (logika token ring)**  
   - Zawiera reprezentację tokena (struktura C) oraz tablicę routingu (mapowanie nazwa węzła → adres/port + następnik).  
   - Implementuje logikę przekazywania tokena, obsługę pustego tokena, osadzanie/odbiór danych użytkownika.

3. **Moduł `join_protocol` (wariant W11)**  
   - Obsługuje komunikaty broadcast związane z dołączaniem nowych węzłów.  
   - Utrzymuje synchronizację z modułem `ring_core`, tak aby nie doszło do wyścigów.

4. **Moduł `cli` / `node_app` (interfejs procesu-węzła)**  
   - Parsuje argumenty linii poleceń (np. `--name`, `--listen-port`, `--broadcast-addr`).  
   - Aplikacja nasłuchuje na 3 gniazdach: broadcast, unicast, CLI (interfejs użytkownika).  
   - CLI udostępnia użytkownikowi prosty interfejs tekstowy (np: `send <node> <msg>`).

5. **Moduł `tests`**  
   - Zawiera scenariusze testowe (np. skrypty uruchamiające kilka węzłów w Docker Compose).  
   - Odpowiada za generowanie logów wykorzystywanych potem w sprawozdaniu.

## Wykorzystane technologie i narzędzia

- Język programowania: C
- Biblioteki: pthread, socket, arpa/inet, podstawowe biblioteki do pracy z językiem C np. stdio.h, stdlib.h, string.h, time.h
- Narzędzia: CMake, Docker Compose, Docker, netem

## Opis najważniejszych rozwiązań funkcjonalnych
W końcowej wersji typ odpowiadający tokenowi wygląda następująco:

```c
typedef struct Token {
    char data[MAX_DATA_SIZE];
    char sender[MAX_NODE_NAME_SIZE]; 
    char receiver[MAX_NODE_NAME_SIZE]; 
    uint32_t topo_version;
    bool is_empty;
} token_t;
```

Wersja topologii pierścienia jest liczbą całkowitą zwiększaną o 1 przy każdej zmianie w pierścieniu (dołączenie nowego procesu). Procesy przy odbiorze tokena będą porównywały wersję pierścienia w tokenie z lokalną wersją. Jeśli wersja w tokenie będzie nowsza to proces wyczyści zawartość swojej kolejki oczekujących do dołączenia (różnica wersji = liczba dołączonych procesów od ostatniego przejścia tokena przez dany proces).


Stan danego proceesu w pierścieniu jest przechowywany w strukturze ring_state_t, struktura ta agreguje inne struktury:
```c
typedef struct Route {
    char node_name[MAX_NODE_NAME_SIZE];
    uint16_t unicast_port;
    uint16_t broadcast_port;
} route_t;

typedef struct RouteConfig {
    route_t* current;
    route_t* prev;
    route_t* next;
} route_config_t;

typedef struct {
    int used; 
    uint32_t request_id; // identyfikator żądania dołączenia
    char node_name[MAX_NODE_NAME_SIZE]; // nazwa procesu chcącego dołączyć
    uint16_t unicast_port; // port unicast procesu chcącego dołączyć
    struct in_addr ip; 
    time_t last_seen; // czas ostatniego otrzymania broadcastu od tego procesu
} pending_join_t;


typedef struct JoinInflight {
    int active; // czy jest aktywne dołączenie procesu
    uint32_t request_id; // identyfikator żądania dołączenia

    pending_join_t joiner; // informacje o procesie, który chce dołączyć

    char expected_prev_name[MAX_NODE_NAME_SIZE]; // oczekiwana nazwa poprzednika
    int got_confirm_prev; // czy otrzymano potwierdzenie od poprzednika
    int got_confirm_joiner; // czy otrzymano potwierdzenie od dołączającego

    time_t last_sent; // czas ostatniego wysłania wiadomości
    int retries; // liczba ponowień wysłania wiadomości
} join_inflight_t;


typedef struct RingState {
    route_config_t config; // zawiera identyfikator sieciowy procesu oraz informacje o poprzedniku i następniku w pierścieniu
    int joined; // czy proces jest w pierścieniu
    uint32_t last_seen_topo_version; // ostatnia znana wersja topologii pierścienia
    uint32_t token_epoch; // liczba przejść tokena przez dany proces
    time_t last_token_seen; // czas ostatniego otrzymania tokena
    int broadcast_socket; // gniazdo do odbioru broadcastów

    join_state_t join_state; // agreguje procesy oczekujące na dołączenie oraz te które zostały już obsłużone
    join_inflight_t join_inflight; // informacje o aktualnie obsługiwanym dołączeniu procesu
    join_ack_sender_t ack_sender; // obsługuje potwierdzenia dołączenia

    // pola wykorzystywane przy dołączaniu procesu do pierścienia
    uint32_t join_request_id; // identyfikator żądania dołączenia
    time_t join_request_last_sent; // czas ostatniego wysłania żądania dołączenia
    int join_request_retries; // liczba ponowień żądania dołączenia
    
    token_t token_in; // otrzymany token
    int have_token; // czy proces posiada token

    token_t cli_pending; // token z danymi od użytkownika czekający na wysłanie
    int have_cli_pending; // czy jest token z danymi od użytkownika czekający na wysłanie
} ring_state_t;
```
Główna funkcja znajduje się w pliku ring.c. Jest to pętla zdarzeń wykorzystująca select do obsługi gniazd UDP (broadcast, unicast) oraz wejścia CLI. W pętli sprawdzane są następujące warunki:
1. Odbiór wiadomości unicast (handle_unicast_if_ready)
2. Odbiór wiadomości broadcast (handle_broadcast_if_ready) i obsługa dołączenia procesu do pierścienia
3. Załadowanie danych od użytkownika do tokena (handle_cli_if_ready), jeśli proces posiada pusty token
4. Przekazanie do funkcji ring_on_token():
   -  jeśli pusty token i są dane od użytkownika do wysłania, to dołącza je do tokena i ustawia flagę have_cli_pending na 0
   -  przekazuje token dalej


Inną ważną funkcją jest unicast_dispatch_message, która rozpoznaje typ otrzymanej wiadomości unicast i wywołuje odpowiednią funkcję obsługi (np. handle_token_unicast, handle_join_accept_u). Warto jeszcze opisać jak wygląda typ unicast_msg_t:
```c
typedef enum {
    UMSG_TOKEN = 1,
    UMSG_JOIN_ACCEPT_U = 2,
    UMSG_JOIN_ACK_U = 4,
    UMSG_JOIN_ACK_ACK_U = 5,
} unicast_msg_type_t;


typedef struct {
    uint16_t type;
    uint16_t payload_len;
    uint8_t payload[MAX_UNICAST_PAYLOAD];
} unicast_msg_t;
int unicast_dispatch_message(ring_state_t* state, const unicast_msg_t* msg, int unicast_socket) {
    if (msg->type == UMSG_TOKEN) {
        handle_token_unicast(state, msg);
        return 0;
    }

    if (msg->type == UMSG_JOIN_ACCEPT_U) {
        return handle_join_accept_u(state, msg, unicast_socket);
    }

    if (msg->type == UMSG_JOIN_ACK_U) {
        return handle_join_ack_u(state, msg, unicast_socket);
    }

    if (msg->type == UMSG_JOIN_ACK_ACK_U) {
        handle_join_ack_ack_u(state, msg);
        return 0;
    }
    return 0;
}
```


Powstał jeszcze plik join_fsm.c, który zawiera funkcje obsługujące stany protokołu dołączania procesu do pierścienia:
```c
int join_fsm_request_tick(ring_state_t* state, int broadcast_socket);
void join_fsm_start_inflight(ring_state_t* state, const pending_join_t* pending);
int join_fsm_inflight_tick(ring_state_t* state, int unicast_socket, int broadcast_socket);
void join_fsm_handle_ack_broadcast(ring_state_t* state, const join_ack_t* ack, int broadcast_socket);
void join_fsm_maybe_complete(ring_state_t* state, int broadcast_socket);
```

Powstała również funkcja do znajdowania aktywnych interfejsów, które udostępniają broadcast w sieci lokalnej - `broadcast_collect_targets` w pliku broadcast.c, korzysta ona z `getifaddrs` oraz wylicza adres broadcast dla każdego interfejsu:
```c
static struct in_addr compute_broadcast(struct in_addr ip, struct in_addr netmask) {
    struct in_addr broadcast;
    broadcast.s_addr = ip.s_addr | ~netmask.s_addr;
    return broadcast;
}
```

Powstała również funkcja `handle_broadcast` w pliku broadcast.c, która obsługuje odebrane wiadomości broadcast, korzysta ona ze switcha na typ wiadomości, która dotyczy dołączenia procesu do pierścienia:
```c
typedef enum {
    JOIN_REQUEST = 1,
    JOIN_COMMIT = 6,
} join_message_type_t;
```
Obsługa JOIN_REQUEST polega na dodaniu procesu do kolejki oczekujących. Obsługa JOIN_COMMIT polega na usunięciu z kolejki procesów, które zostały już dołączone (na podstawie wersji topologii pierścienia).

Obsługa broadcastów może być zawodna, dlatego zdecydowaliśmy się ograniczyć ją do minimum - tylko do obsługi dołączania procesu do pierścienia. W przypadku utraty broadcastu JOIN_REQUEST proces będzie ponawiał wysyłanie żądania dołączenia co określony czas, aż do momentu otrzymania akceptacji.

Przesyłanie tokena oraz wiadomości unicast jest realizowane za pomocą niezawodnego UDP zaimplementowanego w module rudp zgodnie ze schematem określonym w dokumentacji wstępnej. Moduł ten udostępnia funkcje `rudp_send` oraz `rudp_recv`, które obsługują retransmisje i potwierdzenia.


## Opis interfejsu użytkownika
Użytkownik może wchodzić w interakcję z pierścieniem wykorzystując dwa skompilowane pliki wykonywalne - `core` i `writer`. Definiują one następujące interfejsy:
   - `core` - program do rozpoczęcia pracy w pierścieniu. Możliwe są dwa argumenty wywołania, 0 i 1, które odpowiednio wskazują, czy proces będzie wysyłał żądanie do dołączenia, czy znajduje już się w pierścieniu. Niezależnie od tego jaki wariant wybierzemy zdefiniowane muszą być następujące zmienne środowiskowe:
      - `NODE_NAME` - nazwa węzła;
      - `NODE_UNI_PORT` - port nasłuchiwania unicast;
      - `NODE_BROAD_PORT` -port nasłuchiwania broadcast;

      W przypadku, gdy pierścień znajduje się już w pierścieniu (wywołanie z flagą 1) należy dodatkowo rozszerzyć zbiór zmiennych o te związane z tablicą routingu danego węła:
     - `SHOULD_START` - zmienna definiujaca, czy zadany węzeł inicjalizuje pracę pierścienia po przez wysłanie pierwszego znacznika. Dokładnie jeden proces powinnien mieć zapaloną tę flagę.
     - `PREV_NODE_NAME` - nazwa poprzedniego wezła;
     - `PREV_NODE_UNI_PORT` - port nasłuchiwania unicast poprzedniego wezła;
     - `NEXT_NODE_NAME` - nazwa następnego węzła;
     - `NEXT_NODE_UNI_PORT` - port nasłuchiwania unicast następnego węzła.

     Jeżeli proces żąda dołączyć do pierścienia, tablica routingu dla nowego procesu zostanie zdefiniowana automatycznie - dołączanie obsłuży węzęł, który w danej chwili posiada token.

  - `writer` -  program do interakcji użytkownika z pierścieniem, umożliwia wysłanie wiadomości do odbiorcy - innego procesu dołączonego do pierścienia. Podczas wywoływania programu jako argumenty należy podać nazwę węzła skojarzoną z odbiorcą (resolver DNS odpowiednio zmapuje nazwę na adres), a także treść wiadomości w postaci tekstowej. Sygnatura wywołania programu może zostać przedstawiona jako `./writer <data> <receiver_name`. W środowisku skonteneryzowanym za pomocą Dockera uruchomienie wygląda analogicznie, jednakże musi zostać podana także nazwa kontenera `docker exec -it <container_name> /app/build/src/writer <data> <receiver_name>`.

Szczegółowa konfiguracja sieci oraz zdefiniownaych zmiennych środowiskowych znajduje się w plikach `docker-compose`.

## Testy pierścienia (Docker)

### Scenariusze testowe

Testy były wykonywane jako scenariusze Docker Compose. W logach znajdują się
m.in.:

- scenariusz z 3 węzłami pierścienia i 4 dołączającymi węzłami,
- ten sam scenariusz z włączonym netem,
- scenariusz z 7 węzłami tworzącymi stały pierścień,
- wariant 7-węzłowy z netem.

Scenariusze z netem uruchamiano z opóźnieniami i stratami pakietów (delay=1000ms
jitter=500ms packet loss=50%).

### Wysyłanie wiadomości do pierścienia

Aby wpuścić wiadomość do pierścienia, należy uruchomić komendę:

```bash
docker exec -it "nazwa kontenera nadawcy" ./build/src/writer "treść" obiorca
```

### Fragmenty logów

Poniżej znajdują się fragmenty logów z poszczególnych scenariuszy; wpisy mogą
być przeplatane, ponieważ logi pochodzą z wielu równoległych procesów (kilka
kontenerów) i są scalane w trakcie zbierania, ale nadal pokazują przebieg testu:

#### Dołączanie węzłów (wariant 3+4)

Niektóre zduplikowane logi np JOIN_REQUEST zostały usunięte aby skrócić zapis
scenariusza

```c
z11_node0  | [INFO] 20-01-2026 19:51:35: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:51:36: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:51:36: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:51:37: JOIN_REQUEST id=157074605 name=node3 uni=5000 from=172.18.0.2
[...]
z11_node2  | [INFO] 20-01-2026 19:51:37: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:51:37: TOKEN: have_token=1 pending=2 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:37: JOIN_INFLIGHT START: joiner=node3 req=157074605 (prev=node1 curr=node2)
z11_node5  | [INFO] 20-01-2026 19:51:39: Node initalized, waiting for UDP packets
[...]
z11_node6  | [INFO] 20-01-2026 19:51:41: Node initalized, waiting for UDP packets
z11_node0  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node2  | [INFO] 20-01-2026 19:51:43: JOIN_INFLIGHT COMPLETE: new prev=node3 topo=1
z11_node3  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node1  | [INFO] 20-01-2026 19:51:43: JOIN_COMMIT: req=157074605 new=node3 topo=1
z11_node2  | [INFO] 20-01-2026 19:51:45: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:45: JOIN_INFLIGHT START: joiner=node4 req=1227623603 (prev=node3 curr=node2)
z11_node1  | [INFO] 20-01-2026 19:51:49: JOIN_REQUEST id=2769632242 name=node5 uni=5000 from=172.18.0.7
[...]
z11_node1  | [INFO] 20-01-2026 19:51:53: JOIN_COMMIT: req=1227623603 new=node4 topo=2
z11_node2  | [INFO] 20-01-2026 19:51:53: JOIN_INFLIGHT COMPLETE: new prev=node4 topo=2
[...]
z11_node2  | [INFO] 20-01-2026 19:51:57: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:51:57: JOIN_INFLIGHT START: joiner=node5 req=2769632242 (prev=node4 curr=node2)
[...]
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_INFLIGHT COMPLETE: new prev=node5 topo=3
z11_node5  | [INFO] 20-01-2026 19:52:03: JOIN_COMMIT: req=2769632242 new=node5 topo=3
[...]
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_REQUEST id=710564936 name=node6 uni=5000 from=172.18.0.8
z11_node2  | [INFO] 20-01-2026 19:52:03: TOKEN: have_token=1 pending=1 inflight=0
z11_node2  | [INFO] 20-01-2026 19:52:03: JOIN_INFLIGHT START: joiner=node6 req=710564936 (prev=node5 curr=node2)
z11_node6  | [INFO] 20-01-2026 19:52:08: JOIN_COMMIT: req=710564936 new=node6 topo=4
[...]
z11_node2  | [INFO] 20-01-2026 19:52:08: JOIN_INFLIGHT COMPLETE: new prev=node6 topo=4
[...]
z11_node2  | [INFO] 20-01-2026 19:52:14: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:52:15: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:52:15: TOKEN: have_token=1 pending=0 inflight=0
[...]
z11_node1  | [INFO] 20-01-2026 19:52:16: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:52:16: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:52:17: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:52:17: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:52:18: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:52:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:52:19: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:52:19: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:52:20: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:52:20: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:52:21: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:52:21: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:52:22: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:52:22: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:52:23: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:52:23: TOKEN: have_token=1 pending=0 inflight=0
```

Można zauważyć, że na początku zostaje opróżniona kolejka oczekujących na
dołączenie procesów, a dopiero po dołączeniu procesów token wznawia ruch.

#### Dołączanie węzłów z netem

```c
z11_node6  | [INFO] Applying netem: delay=1000ms jitter=500ms loss=50%
z11_node0  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node0  | [INFO] 20-01-2026 19:54:45: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node2  | [INFO] 20-01-2026 19:54:45: Node initalized, waiting for UDP packets
z11_node4  | [INFO] 20-01-2026 19:54:47: Node initalized, waiting for UDP packets
z11_node3  | [INFO] 20-01-2026 19:54:47: Node initalized, waiting for UDP packets
z11_node2  | [INFO] 20-01-2026 19:54:48: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node1  | [INFO] 20-01-2026 19:54:48: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node5  | [INFO] 20-01-2026 19:54:49: Node initalized, waiting for UDP packets
z11_node1  | [INFO] 20-01-2026 19:54:50: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:54:50: TOKEN: have_token=1 pending=1 inflight=0
z11_node1  | [INFO] 20-01-2026 19:54:50: JOIN_INFLIGHT START: joiner=node4 req=4239188680 (prev=node0 curr=node1)
z11_node0  | [INFO] 20-01-2026 19:54:51: JOIN_REQUEST id=4239188680 name=node4 uni=5000 from=172.18.0.5
z11_node6  | [INFO] 20-01-2026 19:54:51: Node initalized, waiting for UDP packets
[...]
z11_node1  | [INFO] 20-01-2026 19:55:03: JOIN_INFLIGHT COMPLETE: new prev=node4 topo=1
[...]
z11_node1  | [INFO] 20-01-2026 19:55:05: JOIN_REQUEST id=882587745 name=node6 uni=5000 from=172.18.0.6
z11_node1  | [INFO] 20-01-2026 19:55:05: JOIN_INFLIGHT START: joiner=node6 req=882587745 (prev=node4 curr=node1)
[...]
z11_node1  | [INFO] 20-01-2026 19:55:09: JOIN_REQUEST id=1332109065 name=node5 uni=5000 from=172.18.0.4
z11_node1  | [INFO] 20-01-2026 19:55:09: TOKEN: have_token=1 pending=1 inflight=1
z11_node1  | [INFO] 20-01-2026 19:55:16: JOIN_INFLIGHT COMPLETE: new prev=node6 topo=2
[...]
z11_node1  | [INFO] 20-01-2026 19:55:20: JOIN_COMMIT: req=4239188680 new=node4 topo=1
z11_node1  | [INFO] 20-01-2026 19:55:20: JOIN_INFLIGHT START: joiner=node5 req=1332109065 (prev=node6 curr=node1)
z11_node1  | [INFO] 20-01-2026 19:55:24: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:26: JOIN_COMMIT: req=882587745 new=node6 topo=2
[...]
z11_node1  | [INFO] 20-01-2026 19:55:29: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:33: JOIN_REQUEST id=3305714384 name=node3 uni=5000 from=172.18.0.7
z11_node1  | [INFO] 20-01-2026 19:55:48: JOIN_INFLIGHT COMPLETE: new prev=node5 topo=3
z11_node1  | [INFO] 20-01-2026 19:55:49: JOIN_INFLIGHT START: joiner=node3 req=3305714384 (prev=node5 curr=node1)
[...]
z11_node1  | [INFO] 20-01-2026 19:55:53: JOIN_COMMIT: req=1332109065 new=node5 topo=3
z11_node1  | [INFO] 20-01-2026 19:56:03: JOIN_INFLIGHT COMPLETE: new prev=node3 topo=4
z11_node1  | [INFO] 20-01-2026 19:56:05: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:56:09: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:56:09: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:56:13: JOIN_COMMIT: req=3305714384 new=node3 topo=4
z11_node0  | [INFO] 20-01-2026 19:56:24: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:56:24: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:56:36: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:56:36: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:56:41: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:56:41: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:56:53: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:56:53: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:57:02: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:57:02: TOKEN: have_token=1 pending=0 inflight=0
```

Tutaj dołączanie przebiega wolniej ze względu na opóźnienia i straty pakietów w
sieci symulowane przez netem. Funkcjonalnie jednak wszystko działa podobnie jak
w scenariuszu bez netem.

#### Stabilna praca pierścienia (7 węzłów)

```c
z11_node0  | [INFO] 20-01-2026 19:45:15: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:45:16: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:45:16: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:45:17: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:45:17: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:45:18: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:45:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:45:19: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:45:19: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:45:20: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:45:20: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:45:21: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:45:21: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:45:22: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:45:22: TOKEN: have_token=1 pending=0 inflight=0
```

Można zauważyć, że token krąży bez przeszkód pomiędzy wszystkimi węzłami
pierścienia.

#### Stabilna praca z netem

```c
z11_node2  | [INFO] Applying netem: delay=1000ms jitter=500ms loss=50%
z11_node0  | [INFO] 20-01-2026 19:48:24: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:48:39: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:48:39: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:48:51: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:48:51: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:48:58: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:48:58: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:49:13: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:49:13: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:49:25: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:49:25: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:49:37: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:49:37: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:49:51: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:49:51: TOKEN: have_token=1 pending=0 inflight=0
```

Netem wprowadza opóźnienia i straty pakietów, ale token nadal krąży pomiędzy
węzłami.

#### Dodanie wiadomości do pierścienia

Wiadomość została wysłana za pomocą komendy

```bash
docker exec -it z11_node1 ./build/src/writer "kapitan 44" node0
```

```c
z11_node1  | [INFO] 20-01-2026 19:37:56: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:37:56: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 20-01-2026 19:37:56: Attaching CLI token: kapitan 44
z11_node1  | 
z11_node2  | [INFO] 20-01-2026 19:37:58: Received token via unicast
z11_node2  | [INFO] 20-01-2026 19:37:58: TOKEN: have_token=1 pending=0 inflight=0
z11_node2  | [INFO] 20-01-2026 19:37:58: Full token received for another node - forwarding
z11_node2  | 
z11_node3  | [INFO] 20-01-2026 19:38:15: Received token via unicast
z11_node3  | [INFO] 20-01-2026 19:38:15: TOKEN: have_token=1 pending=0 inflight=0
z11_node3  | [INFO] 20-01-2026 19:38:15: Full token received for another node - forwarding
z11_node3  | 
z11_node4  | [INFO] 20-01-2026 19:38:18: Received token via unicast
z11_node4  | [INFO] 20-01-2026 19:38:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 20-01-2026 19:38:18: Full token received for another node - forwarding
z11_node4  | 
z11_node5  | [INFO] 20-01-2026 19:38:20: Received token via unicast
z11_node5  | [INFO] 20-01-2026 19:38:20: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 20-01-2026 19:38:20: Full token received for another node - forwarding
z11_node5  | 
z11_node6  | [INFO] 20-01-2026 19:38:22: Received token via unicast
z11_node6  | [INFO] 20-01-2026 19:38:22: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 20-01-2026 19:38:22: Full token received for another node - forwarding
z11_node6  | 
z11_node0  | [INFO] 20-01-2026 19:38:24: Received token via unicast
z11_node0  | [INFO] 20-01-2026 19:38:24: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 20-01-2026 19:38:24: Received token for me: kapitan 44
z11_node0  | 
z11_node1  | [INFO] 20-01-2026 19:38:29: Received token via unicast
z11_node1  | [INFO] 20-01-2026 19:38:29: TOKEN: have_token=1 pending=0 inflight=0
```

`node1` wprowadza wiadomość do pierścienia, a `node0` ją odbiera po przejściu
przez pozostałe węzły.

#### Dodanie wiadomości do pierścienia kiedy token jest pełny

Do wysłania wielu wiadomości został użyty skrypt `send-mess.sh`, który wysyła
wiele wiadomości, w tym kilka z tego samego węzła

```bash
docker exec -it z11_node2 ./build/src/writer "2 do 0" node0
docker exec -it z11_node1 ./build/src/writer "1 do 5" node5
docker exec -it z11_node1 ./build/src/writer "1 do 0" node0
docker exec -it z11_node0 ./build/src/writer "0 do 6" node6
docker exec -it z11_node4 ./build/src/writer "4 do 5" node5
docker exec -it z11_node1 ./build/src/writer "1 do 6" node6
```

```c
z11_node4  | [INFO] 21-01-2026 21:27:07: Received token via unicast
z11_node4  | [INFO] 21-01-2026 21:27:07: TOKEN: have_token=1 pending=0 inflight=0
z11_node4  | [INFO] 21-01-2026 21:27:07: Attaching CLI token: 4 do 5
z11_node4  | 
z11_node5  | [INFO] 21-01-2026 21:27:08: Received token via unicast
z11_node5  | [INFO] 21-01-2026 21:27:08: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 21-01-2026 21:27:08: Received token for me: 4 do 5
z11_node5  | 
z11_node6  | [INFO] 21-01-2026 21:27:09: Received token via unicast
z11_node6  | [INFO] 21-01-2026 21:27:09: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 21-01-2026 21:27:10: Received token via unicast
z11_node0  | [INFO] 21-01-2026 21:27:10: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 21-01-2026 21:27:10: Attaching CLI token: 0 do 6
[...]
z11_node6  | [INFO] 21-01-2026 21:27:16: Received token via unicast
z11_node6  | [INFO] 21-01-2026 21:27:16: TOKEN: have_token=1 pending=0 inflight=0
z11_node6  | [INFO] 21-01-2026 21:27:16: Received token for me: 0 do 6
z11_node6  | 
z11_node0  | [INFO] 21-01-2026 21:27:17: Received token via unicast
z11_node0  | [INFO] 21-01-2026 21:27:17: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 21-01-2026 21:27:18: Received token via unicast
z11_node1  | [INFO] 21-01-2026 21:27:18: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 21-01-2026 21:27:18: Attaching CLI token: 1 do 5
[...]
z11_node5  | [INFO] 21-01-2026 21:27:22: Received token via unicast
z11_node5  | [INFO] 21-01-2026 21:27:22: TOKEN: have_token=1 pending=0 inflight=0
z11_node5  | [INFO] 21-01-2026 21:27:22: Received token for me: 1 do 5
[...]
z11_node1  | [INFO] 21-01-2026 21:27:25: Attaching CLI token: 1 do 0
[...]
z11_node0  | [INFO] 21-01-2026 21:27:31: Received token via unicast
z11_node0  | [INFO] 21-01-2026 21:27:31: TOKEN: have_token=1 pending=0 inflight=0
z11_node0  | [INFO] 21-01-2026 21:27:31: Received token for me: 1 do 0
z11_node0  | 
z11_node1  | [INFO] 21-01-2026 21:27:32: Received token via unicast
z11_node1  | [INFO] 21-01-2026 21:27:32: TOKEN: have_token=1 pending=0 inflight=0
z11_node1  | [INFO] 21-01-2026 21:27:32: Attaching CLI token: 1 do 6
[...]
z11_node6  | [INFO] 21-01-2026 21:27:37: Received token for me: 1 do 6
[...]
z11_node2  | [INFO] 21-01-2026 21:27:40: Attaching CLI token: 2 do 0
[...]
z11_node0  | [INFO] 21-01-2026 21:27:45: Received token for me: 2 do 0
```

Mimo wielu nadawców token przechowuje jedną wiadomość na raz i nie jest
nadpisywany - każda wiadomość trafia do tokena dopiero po jego opróżnieniu,
dzięki czemu kolejne wpisy są rozdzielone w czasie i docierają do właściwych
odbiorców w kolejnych obiegach pierścienia.


