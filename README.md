# DeodeokOS
[64비트 멀티코어 OS 원리와 구조](https://www.hanbit.co.kr/store/books/look.php?p_code=B3548683222) 서적을 바탕으로 작성된 OS 프로젝트 입니다.

*개인 기록용이므로 잦은 변경 및 오류가 있을 수 있습니다!*
# 환경 및 구축
- Apple M1 Macbook Air
- Docker(ubuntu 18.04 / amd64)
- gcc, qemu...
가
x86_64 환경에서 작성된 OS 이므로 M1인 경우 docker의 platform 옵션을 추가하셔야 합니다.

```
docker build --platform linux/amd64 -t os:1.0 .
docker-compose up -d
docker exec -it os bash
./build.sh && ./qemu.sh
```

# 참고사항
- '64비트 멀티코어 OS 원리와 구조' 서적에서 소개하는 QEMU 환경이 0.10.4 버전이라 최신 QEMU를 사용한다면 OS가 제대로 올라오지 않습니다. 필자는 QEMU 2 버전대를 쓰고 있으며 이를 해결하려면 부트로더의 수정이 필요합니다. [Bootloader.asm](https://github.com/redundant4u/DeodeokOS/blob/main/volumes/00.BootLoader/BootLoader.asm)의 80번 줄 ```cmp al, 19```를 ```cmp al, 37```로 변경하면 작동이 됩니다.

# 디버깅
## gdb
gdb를 통해 디버깅을 진행한다면 2개의 쉘이 필요합니다.
```bash
docker exec -it os bash
./build.sh && ./qemu_debug.sh
```

```bash
gdb
target remote:1234
file 02.Kernel64/Temp/Kernel64.elf
```

breakpoint 설정 후 디버깅 진행

## vscode
먼저 [Native Debug extension](https://marketplace.visualstudio.com/items?itemName=webfreak.debug)을 설치 합니다.

vscode 환경에서 디버깅을 진행한다면 다음의 과정을 따릅니다.
1. ```.vscode/task.json```의 QEMU Build 실행
2. ```.vscode/task.json```의 QEMU Debug 실행
3. breakpoint 찍기
4. F5으로 디버깅 진행

```.vscode/launch.json```과 ```.vscode/task.json``` 내용을 수정하여 명령어 실행을 커스텀 할 수 있습니다.

# 변경점
- 211116: 멀티코어 구현2(코어 활성화)
- 211028: 멀티코어 구현1(MP 테이블 구현현
- 211020: 시리얼 포트 디바이스 드라이버 구현
- 211017: 파일 시스템 캐시 및 램디스크 구현
- 211002: 파일 입출력 구현
- 210927: 디버깅 환경 구성
- 210902: 간단한 파일 시스템 구현
- 210830: 하드디스크 드라이버 추가
- 210824: 동적 메모리 할당 구현
- 210813: 멀티스레딩 구현
- 210809: Mutex를 통한 race condition 해결
- 210807: 멀티 레벨 큐 스케줄러 구현
- 210802: 라운드 로빈 스케줄러 구현
- 210725: 간단한 멀티태스킹 구현
- 210723: 타이머 디바이스 추가
- 210720: 콘솔 쉘 구현
- 210719: PIC 컨트롤러를 이용한 인터럽트 구현
- 210717: 기본 키보드 인터럽트 추가(polling 방식)
- 210716: 키보드 드라이버 추가
- 210712: 보호모드에서 IA-32e 모드로 전환
- 210708: 첫 번째 커널 작성
- 210706: 리얼모드에서 보호모드로 전환
- 210701: 부트로더 추가