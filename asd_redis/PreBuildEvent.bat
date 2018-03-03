@echo off

set print="%~dp0include\asd\version.h"

echo #pragma once > %print%
echo // 이 매크로는 asd_redis를 참조하는 다른 프로젝트들이 새로 빌드된 asd_redis.dll을 자동으로 감지하여 적용시키기 위한 장치입니다. >> %print%
echo // 아래와 같은 매커니즘으로 동작합니다. >> %print%
echo //  1. asd_redis가 새로 빌드될 때 asd_RedisWrapperVersion 값 갱신 >> %print%
echo //  2. asd_redis를 참조하는 프로젝트들은 이를 include하고 있으므로 새로 빌드가 필요해짐 >> %print%
echo //  3. 빌드 중 자동으로 실행되는 빌드이벤트가 asd_redis.dll을 자신의 경로로 복사 >> %print%
echo #define asd_RedisWrapperVersion "%DATE% %TIME%" >> %print%