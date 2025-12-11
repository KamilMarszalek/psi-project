# PSI Projekt 2025Z
## Autorzy:
- Damian D'Souza (lider)
- Kamil Marszałek
- Michał Szwejk

## Temat projektu

Program realizujący komunikację w logicznym pierścieniu (token ring) zbudowany w oparciu o protokół UDP.


## Treść

Napisać program realizujący komunikację w logicznym pierścieniu (token ring) w oparciu o protokół UDP.

---

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

---

### Warianty funkcjonalne

(Każdy zespół otrzyma jeden z wariantów W1 i W2.  
Wariant może modyfikować założenia podane wyżej, wtedy oczywiście ważniejsze są założenia z wariantu.)

- **W11** – Wprowadzić dodatkową funkcję dołączenia procesu do pierścienia. Powinno odbywa się to poprzez broadcast (rozgłaszanie). Węzeł (proces) zgłasza chęć akcesu do komunikacji, zostaje to potwierdzone i w efekcie zmodyfikowana zostaje lokalna tablica rutingu w określonych węzłach (rysunek 1). Realizacja przez dodatkowy „miniprotokół”, możliwych rozwiązań jest wiele, np. dołączenia może dokonać ten proces, który ma aktualnie znacznik. Uwaga – należy tak zaprojektować protokół, aby uniknąć wyścigów i innych niejednoznaczności.

---

### Warianty implementacyjne  
- **W22** – implementacja w C/C++  

---

## Interpretacja treści zadania
Celem projektu jest stworzenie aplikacji komunikującej się w logicznym pierścieniu za pomocą protokołu UDP. Aplikacja będzie obsługiwać przekazywanie tokena między procesami, umożliwiając im wysyłanie i odbieranie danych. Dodatkowo, aplikacja będzie implementować mechanizm dołączania nowych procesów do pierścienia za pomocą broadcastu, zapewniając aktualizację tablic routingu i unikając konfliktów.

Funkcje:
- Implementacja niezawodnego transferu UDP za pomocą protokołu BAP.
- Obsługa logiki pierścienia komunikacyjnego, w tym przekazywanie tokena i adresacja.
- Mechanizm dołączania nowych procesów do pierścienia poprzez broadcast.
- Prosty interfejs tekstowy do interakcji z użytkownikiem.

## Opis funkcjonalny (black-box)

System PSI Token Ring udostępnia następujące funkcje widoczne z zewnątrz:

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

### Idea na działanie protokołu dołączenia procesu do pieścienia
1. Proces chce dołączyć do pierścienia wysyła broadcast - chce dołączyć do pierścienia.
2. Procesy aktualnie działające otrzymują broadcast - zostaje podniesiona zmienna warunkowa nazwijmy ją - someone_wanna_join
3. Jeśli proces jest już w trakcie wysyłania to ma zaciągnięty mutex oraz zmienną warunkową - is_sending. Kończy on wysyłanie. 
4. Jeśli proces nie jest w trakcie wysyłania is_sending nie jest zaciągnięte to obsługujemy dołączenie nowego procesu - odsyłamy broadcast do procesu potomnego podając mu adresy: skąd i dokąd
5. Tego samego broadcasta dostaje proces który jest już w pierścieniu on aktualizuje swój routing - teraz będzie otrzymywał wiadomości od nowego procesu
6. Nowy proces ustawia swój routing i odsyła do wszystkich broadcast - jestem zapisany - zmienna warunkowa someone_wanna_join podniesiona u wszystkich
7. Procesy które otrzymały broadcast - jestem zapisany - opuszczają tryb dołączania i wracają do normalnej pracy
8. Nowy proces czeka na token i zaczyna normalną pracę

### Opis struktur danych protokołu dołączenia procesu do pieścienia
- Struktura Token:
```c
typedef struct {
    char sender[32];        // Nazwa nadawcy
    char receiver[32];      // Nazwa odbiorcy
    char data[256];        // Dane użytkownika
    uint8_t is_empty;      // Flaga pustego tokena
} Token;
```
- Struktura Routing Table Entry:
```c
typedef struct {
    char node_name[32];        // Nazwa węzła
    unsigned short port;   // Port węzła
    Entry* successor;    // Wskaźnik na następny wpis w tablicy routingu
    Entry* predecessor;  // Wskaźnik na poprzedni wpis w tablicy routingu   
} Entry;
```
- Struktura Broadcast Message:
```c
typedef struct {
    char node_name[32];    // Nazwa węzła dołączającego
    unsigned short port; // Port węzła dołączającego
} BroadcastMessage;
```
## Planowany podział na moduły i struktura komunikacji

1. **Moduł `reliable_udp` (BAP)**  
   - Odpowiada za niezawodny transfer między dwoma endpointami UDP.  
   - Udostępnia API w stylu: `rudp_send()`, `rudp_recv()`, które wewnętrznie realizują potwierdzenia, numery sekwencyjne, timeouty i retransmisje.

2. **Moduł `ring_core` (logika token ring)**  
   - Zawiera reprezentację tokena (struktura C) oraz tablicę routingu (mapowanie nazwa węzła → adres/port + następnik).  
   - Implementuje logikę przekazywania tokena, obsługę pustego tokena, osadzanie/odbiór danych użytkownika.

3. **Moduł `join_protocol` (wariant W11)**  
   - Obsługuje komunikaty broadcast związane z dołączaniem nowych węzłów.  
   - Realizuje prostą maszynę stanów: `IDLE → JOIN_REQUEST_RECEIVED → ROUTING_UPDATE → JOIN_CONFIRMED`.  
   - Utrzymuje synchronizację z modułem `ring_core`, tak aby w trakcie dołączania nie powstał drugi token.

4. **Moduł `cli` / `node_app` (interfejs procesu-węzła)**  
   - Parsuje argumenty linii poleceń (np. `--name`, `--listen-port`, `--broadcast-addr`).  
   - Uruchamia odpowiednie wątki: wątek obsługi tokena i wątek obsługi broadcastów.  
   - Udostępnia użytkownikowi prosty interfejs tekstowy (np. komendy: `send <node> <msg>`, `show-routing`, `quit`).

5. **Moduł `tests`**  
   - Zawiera scenariusze testowe (np. skrypty uruchamiające kilka węzłów w Docker Compose).  
   - Odpowiada za generowanie logów wykorzystywanych potem w sprawozdaniu.


## Zarys koncepcji implementacji
- Język programowania: C
- Biblioteki: pthreads, sockets, resolver
- Narzędzia: CMake, Docker Compose