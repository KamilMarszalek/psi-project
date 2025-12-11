# PSI Projekt 2025Z
## Autorzy:
- Damian D'Souza (lider)
- Kamil Marszałek
- Michał Szwejk

## Temat projektu

Program realizujący komunikację w **logicznym pierścieniu (token ring)** zbudowany w oparciu o protokół UDP.

---

## Treść zadania

Celem projektu jest zaimplementowanie systemu komunikacji między procesami w topologii **logicznego pierścienia**. Węzły komunikują się za pomocą protokołu **UDP**, a dane są przekazywane w postaci specjalnego pakietu – **tokenu**.

### Założenia ogólne

- W komunikacji uczestniczą **węzły/procesy** (do testów co najmniej trzy), przy czym liczba węzłów może się zmieniać.  
- Każdy węzeł identyfikowany jest **krótką nazwą ASCII**. Należy mapować adresy IP wezłów na ich logiczne nazwy.
- Komunikacja odbywa się poprzez pakiet danych tzw. **token**, który jest cyklicznie przekazywany między procesami zgodnie z kolejnością w pierścieniu.  
- Jeśli węzeł A chce wysłać dane do węzła B:
  - czeka na **pusty token** (token bez danych),
  - uzupełnia token o **adres odbiorcy** oraz **dane do przesłania**,
  - przekazuje token do swojego następcy w pierścieniu.  
- W pierścieniu musi znajdować się **dokładnie jeden token** przez cały czas działania systemu.  

### Niezawodność komunikacji

- Ponieważ używany jest **UDP**, konieczne jest wprowadzenie mechanizmu zapewniającego **niezawodny transfer** na poziomie połączenia między sąsiadami w pierścieniu (węzeł–węzeł).  
- Można wykorzystać **BAP** lub zaprojektować **podobny mechanizm** (potwierdzenia, ponowne wysyłanie, timeouty itp.).

### Struktura programu

Program powinien być **modularny**. Sugerowane moduły:

1. Moduł „**niezawodny UDP**” – implementacja BAP lub podobnego protokołu zapewniającego niezawodność.  
2. Moduł **pierścienia komunikacyjnego** – obsługa tokenu, adresacji, routingu i przekazywania danych.  
3. Moduł **transportu danych użytkownika** w pierścieniu.  
4. Moduł **testów i obsługi linii komend (cmd-line)**.

Dodatkowe założenia:

- Do testów można przyjąć, że przesyłane są **dane tekstowe**.  
- Między odebraniem a przekazaniem tokenu należy wprowadzić **sztuczne opóźnienie**, np. z zakresu **1–5 s**.  
- Wystarczy **tekstowy interfejs użytkownika**:
  - tryb pracy interaktywnej (prosty cmd-line),
  - oraz możliwość wykonywania **testów wsadowych** (bez interakcji użytkownika).  

---

## Wariant funkcjonalny W11 – dołączanie procesu do pierścienia

Należy dodać **funkcję dynamicznego dołączania nowego procesu do pierścienia** z wykorzystaniem mechanizmu **broadcast (rozgłaszanie)**.

Wymagania szczegółowe:

- Nowy węzeł zgłasza chęć **dołączenia do komunikacji** poprzez wysłanie odpowiedniego komunikatu broadcast.  
- Żądanie dołączenia musi zostać **potwierdzone** przez istniejące węzły.  
- W wyniku poprawnego dołączenia **aktualizowane są lokalne tablice routingu** w odpowiednich węzłach, tak aby nowy proces został włączony w pierścień (zmienia się następnik/poprzednik).  
- Dołączenie powinno być zrealizowane jako **dodatkowy „miniprotokół”** (zestaw komunikatów i reguł postępowania).  
- Możliwych rozwiązań jest wiele, np. dołączenia może dokonywać **proces, który aktualnie posiada token**.  
- Protokół musi być zaprojektowany tak, aby **unikać wyścigów i niejednoznaczności**, np. sytuacji zduplikowania tokenu lub niespójnych tablic routingu.  

---

## Wariant implementacyjny W22 – implementacja w C/C++

Wariant W22 oznacza, że:

- Program musi być zaimplementowany w języku C lub C++.  
- Środowiskiem docelowym jest **Linux**.  
- Prezentacja działania projektu powinna odbywać się w środowisku obejmującym **co najmniej trzy maszyny połączone siecią** (mogą to być np. maszyny wirtualne, środowisko kontenerowe lub serwer bigubu).  


## Idea na działanie protokołu dołączenia procesu do pieścienia
1. Proces chce dołączyć do pierścienia wysyła broadcast - chce dołączyć do pierścienia.
2. Procesy aktualnie działające otrzymują broadcast - zostaje podniesiona zmienna warunkowa nazwijmy ją - someone_wanna_join
3. Jeśli proces jest już w trakcie wysyłania to ma zaciągnięty mutex oraz zmienną warunkową - is_sending. Kończy on wysyłanie. 
4. Jeśli proces nie jest w trakcie wysyłania is_sending nie jest zaciągnięte to obsługujemy dołączenie nowego procesu - odsyłamy broadcast do procesu potomnego podając mu adresy: skąd i dokąd
5. Tego samego broadcasta dostaje proces który jest już w pierścieniu on aktualizuje swój routing - teraz będzie otrzymywał wiadomości od nowego procesu
6. Nowy proces ustawia swój routing i odsyła do wszystkich broadcast - jestem zapisany - zmienna warunkowa someone_wanna_join podniesiona u wszystkich
7. Procesy które otrzymały broadcast - jestem zapisany - opuszczają tryb dołączania i wracają do normalnej pracy
8. Nowy proces czeka na token i zaczyna normalną pracę
