# uaRO Graphics 0.1.0 — проверка

Эта версия ещё ничего не рисует сама: она встаёт на место dgVoodoo и передаёт ему всё как есть. Проверяем, что
игра запускается через наш ddraw.dll, ROExt и геймпад не ломаются, а dgVoodoo работает из подпапки.

## Установка (в копию папки клиента)

1. Сделай копию папки клиента, оригинал не трогай.
2. В копии создай папку `dgvoodoo` и **перенеси** в неё `DDraw.dll` и `D3DImm.dll`. Туда же **скопируй**
   `dgVoodoo.conf` (оригинал оставь на месте).
3. Положи рядом с `uaRO.exe` файлы `ddraw.dll` и `uaro-graphics.ini` из архива.

## Проверка 1: всё как раньше

Запусти игру, зайди персонажем, походи, открой окна, Alt+Tab туда и обратно, выйди из игры.

- Игра выглядит и работает как раньше (тот же dgVoodoo).
- ROExt работает как раньше (окно, мышь); с геймпадом — геймпад тоже.
- В папке появился `uaro-graphics.log`. В нём есть строки:
  - `uaRO Graphics 0.1.0 in …\uaRO.exe`
  - `settings: uaro-graphics.ini; backend = dgvoodoo, …`
  - `renderer: dgVoodoo (backend = dgvoodoo)`
  - `passing DirectDraw to …\dgvoodoo\DDraw.dll`
  - `DirectDrawCreateEx = 0x00000000`
  - в конце — `D3DImm.dll: …` (из какой папки dgVoodoo взял D3DImm.dll)

## Проверка 2: что видит наш драйвер

В `uaro-graphics.ini` поставь `backend = uaro`, запусти игру ещё раз. Игра снова должна пойти через dgVoodoo, а в
логе появятся строки:
- `Direct3D 12: OK, adapter NVIDIA GeForce RTX 5060 Ti …`
- `renderer: dgVoodoo (backend = uaro, but the uaRO renderer is not in this version)`

## Проверка 3: без dgVoodoo

Временно переименуй папку `dgvoodoo` и запусти игру: она должна пойти через системный DirectDraw (может выглядеть
хуже — это нормально), в логе `renderer: system DirectDraw (…; dgvoodoo\DDraw.dll not found)`. Верни имя папки.

## Проверка 4: без прав записи (по желанию)

Если копия лежит в `Program Files` и игра запускается без прав администратора, лог может не создаться — игра всё
равно должна запуститься.

## Что прислать

`uaro-graphics.log` после проверок 1 и 2 и коротко: всё ли работало как раньше.
