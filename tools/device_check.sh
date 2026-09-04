#!/bin/sh
# Снимок состояния ридера для ручного тестирования.
# Запуск: sh tools/device_check.sh   (устройство должно быть примонтировано)
DEV=${DEVICE:-/Volumes/NO NAME}
DIR="$DEV/system/pocketbook-statistics"
DB="$DIR/statistics.db"

[ -d "$DIR" ] || { echo "Устройство не примонтировано: $DEV"; exit 1; }

echo "=== версия и последние старты"
grep "app: start" "$DIR/app.log" 2>/dev/null | tail -3
echo
echo "=== app.log (последние 20 строк)"
tail -20 "$DIR/app.log" 2>/dev/null
echo
echo "=== open.log (последние 10 строк)"
tail -10 "$DIR/open.log" 2>/dev/null
echo
echo "=== pidfile: $(cat "$DIR/statistics.pid" 2>/dev/null)"
echo "=== стартов демона в логе: $(grep -c 'daemon: started' "$DIR/app.log" 2>/dev/null)"
echo "=== heartbeat'ов: $(grep -c 'daemon: alive' "$DIR/app.log" 2>/dev/null)"
echo

TMP=$(mktemp -t devcheck).db
cp "$DB" "$TMP" || exit 1

echo "=== сессии за последние 3 суток"
sqlite3 -header -column "$TMP" "
SELECT book_id AS bk,
       datetime(start_time,'unixepoch','localtime') AS start,
       datetime(end_time,'unixepoch','localtime')   AS finish,
       (end_time-start_time)/60 AS span_min,
       active_seconds/60        AS credited_min,
       pages_start AS p0, pages_end AS p1,
       (pages_end-pages_start)  AS pages,
       recovered   AS rec
FROM sessions
WHERE end_time > strftime('%s','now') - 3*86400
ORDER BY end_time;"
echo
echo "=== по дням (так это видит статистика)"
sqlite3 -header -column "$TMP" "
SELECT date(end_time,'unixepoch','localtime') AS day,
       SUM(active_seconds)/60 AS minutes,
       COUNT(*) AS sessions,
       SUM(recovered) AS recovered
FROM sessions
WHERE end_time > strftime('%s','now') - 14*86400
GROUP BY day ORDER BY day;"
echo
echo "=== строки, пересекающие полночь (должно быть пусто на 1.6.3+)"
sqlite3 -column "$TMP" "
SELECT datetime(start_time,'unixepoch','localtime'),
       datetime(end_time,'unixepoch','localtime')
FROM sessions
WHERE date(start_time,'unixepoch','localtime')
   <> date(end_time,'unixepoch','localtime');"
echo
echo "=== подозрительное: кредит больше собственного интервала"
sqlite3 -column "$TMP" "
SELECT datetime(start_time,'unixepoch','localtime'), active_seconds, end_time-start_time
FROM sessions
WHERE active_seconds > (end_time-start_time)+1;"
rm -f "$TMP"
