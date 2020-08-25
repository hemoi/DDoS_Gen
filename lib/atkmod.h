#pragma once
#include "attacktable.h"
#include "pktclass.h"
#include <arpa/inet.h>
#include <thread>
#include <mutex>
#include <memory>
#include <unistd.h>

// TODO: make attack module class
class ATTACKMODULE{
    private:        
        uint16_t bps;
        char* dev;
        int atktype;
        int speed;
        int dur;

        // for http socket
        int sd;

        // attack information structure //
        mac_t target_mac[ETHER_ADDR_LEN];
        ip_t target_ip;

        // private functions
        void attack_routine();
        
        // target의 맥주소 받기
        void get_targetinfo(std::string tip);

    public:
        ATTACKMODULE(char* dev, std::string tip, int type, int speed, int dur);
        
        // for HTTP ATTACK
        void more_setting();

        // 외부에서 호출될 함수
        void attack();

        // HTTP 공격은 별도로 분리
        void http_attack();
};