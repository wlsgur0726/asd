@echo off

set print="%~dp0include\asd\version.h"

echo #pragma once > %print%
echo // �� ��ũ�δ� asd_redis�� �����ϴ� �ٸ� ������Ʈ���� ���� ����� asd_redis.dll�� �ڵ����� �����Ͽ� �����Ű�� ���� ��ġ�Դϴ�. >> %print%
echo // �Ʒ��� ���� ��Ŀ�������� �����մϴ�. >> %print%
echo //  1. asd_redis�� ���� ����� �� asd_RedisWrapperVersion �� ���� >> %print%
echo //  2. asd_redis�� �����ϴ� ������Ʈ���� �̸� include�ϰ� �����Ƿ� ���� ���尡 �ʿ����� >> %print%
echo //  3. ���� �� �ڵ����� ����Ǵ� �����̺�Ʈ�� asd_redis.dll�� �ڽ��� ��η� ���� >> %print%
echo #define asd_RedisWrapperVersion "%DATE% %TIME%" >> %print%