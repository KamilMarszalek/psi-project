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

## Opis funkcjonalny
1. Procesy zostają zainicjalizowane - otwierają trzy gniazda UDP - jedno do odbierania wiadomości, drugie do przekazywania wiadomości do następnego procesu w pierścieniu, trzecie do odbierania broadcastów.
2. Procesy tworzą dwa wątki - jeden do obsługi odbierania i wysyłania wiadomości, a drugi do obsługiwania broadcastów.
3. Procesy komunikują się za pomocą tokena, który jest przekazywany między nimi.
4. Procesy mogą wysyłać dane do innych procesów, doczepiając je do tokena.
5. Procesy mogą dołączać do pierścienia poprzez wysłanie broadcastu z informacją o chęci dołączenia.
6. Procesy aktualizują swoje tablice routingu po otrzymaniu informacji o nowym procesie.
7. Komunikacja jest zrealizowana w sposób niezawodny, zapewniając integralność przesyłanych danych. Używamy w tym celu protokołu BAP.

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

## Planowany podział na moduły i struktuta komunikacji między nimi

## Zarys koncepcji implementacji
- Język programowania: C
- Biblioteki: pthreads, sockets, resolver
- Narzędzia: CMake, Docker Compose