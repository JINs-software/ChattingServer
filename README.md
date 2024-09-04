### \[채팅 서버 성능/오류 테스트\]
* 7일간의 채팅 성능 테스트
  
#### 테스트 조건
* 테스트 조건
  * 메모리 사용량 500MB 이하
  * Update Thread Message TPS: 12000 ~ 16000 내외로 유지
  * Update Thread Message Queue: 평소 0 유지 아주 가끔 ~ 오름
  * 더미 프로그램(프로카데미 제공)
    * 더미 그룹1: 5000명 재접속 모드
    * 더미 그룹2: 5000명 재접속 모드
    * 더미 그룹3: 5000명 연결 유지 모드
  * 채팅 서버 측 Accept TPS: 1200 ~ 1700
  * 채팅 더미 측 Action Delay Avr: ~ 150ms 이하
 
#### 테스트 결과
* 모니터링 클라이언트
  ![image](https://github.com/user-attachments/assets/a981ee98-b430-421c-bca4-1cb048256cd7)
* 더미 그룹 중지 전
  ![image](https://github.com/user-attachments/assets/9f6c3e76-d65f-40c4-870b-c2f05d044446)
* 더미 그룹 중지 후
  ![image](https://github.com/user-attachments/assets/ff4a652b-325d-4634-8409-48b17a5fc306)
