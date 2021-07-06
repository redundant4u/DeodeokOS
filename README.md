# DeodeokOS
[64비트 멀티코어 OS 원리와 구조](https://www.hanbit.co.kr/store/books/look.php?p_code=B3548683222)를 서적을 바탕으로 작성된 OS 프로젝트 입니다.

*개인 기록용이므로 잦은 변경 및 오류가 있을 수 있습니다!*
# 환경 및 구축
- Apple M1 Macbook Air
- Docker(ubuntu 18.04 / amd64)
- gcc, qemu...

x86_64 환경에서 작성된 OS 이므로 M1인 경우 docker의 platform 옵션을 추가하셔야 합니다.

```
docker build --platform linux/amd64 -t os:1.0 .
docker-compose up -d
docker exec -it os bash
./auto.sh
```

# 변경점
- 210701: 부트로더 추가
- 210706: 리얼모드에서 보호모드로 전환