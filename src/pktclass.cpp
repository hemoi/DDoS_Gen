#include "../lib/pktclass.h"

PKT::PKT(char* dev){
    this->dev = dev;
    srand(time(NULL));
}


PKT::~PKT(){
	pcap_close(pcap_handler);      
}

void PKT::set_pcap(){
	pcap_handler = pcap_open_live(dev, BUFSIZ, 1, 1, errbuf);
	if (pcap_handler == nullptr) {
		fprintf(stderr, "couldn't open device %s(%s)\n", dev, errbuf);
		exit(-1);
	}
}

// target의 맥주소를 알아내오기 위해 1번 쓰이는 함수 //
void PKT::set_attackinfo(ip_t target_ip, mac_t* storage){
    ETHARPHDR test_pkt;
    memset(&test_pkt, 0, sizeof(ETHARPHDR));
    make_MAC_byte(BROADCAST_MAC, test_pkt.eth_hdr.ether_dhost);
    get_my_mac(dev, test_pkt.eth_hdr.ether_shost);
    test_pkt.eth_hdr.ether_type = htons(ETHERTYPE_ARP);
    test_pkt.arp_hdr.ar_hrd = htons(ARPHRD_ETHER);
    test_pkt.arp_hdr.ar_pro = htons(ETHERTYPE_IP);         
    test_pkt.arp_hdr.ar_hln = ETHER_ADDR_LEN; 
    test_pkt.arp_hdr.ar_pln = sizeof(in_addr_t);
    test_pkt.arp_hdr.ar_op = htons(ARPOP_REQUEST);
    get_my_mac(dev, test_pkt.srcmac);
    test_pkt.srcip = get_my_ip(dev);
    make_MAC_byte(DONTKNOW_MAC, test_pkt.dstmac);
    test_pkt.dstip = target_ip;

    pktsize = sizeof(ETHARPHDR);
    pkt_ptr = (const uint8_t*)&test_pkt;

    send_packet();

    while(true){
		struct pcap_pkthdr* header;
		ETHARPHDR* packet_ptr;
		
		// listening packets
		pcap_res = pcap_next_ex(pcap_handler, &header, (const u_char**)(&packet_ptr));
		if (pcap_res == 0) continue;
		if (pcap_res == -1 || pcap_res == -2) {
			printf("pcap_next_ex return %d(%s)\n", pcap_res, pcap_geterr(pcap_handler));
			break;
		}
		
		// check if the packet is right
		if((ntohs(packet_ptr->eth_hdr.ether_type) == ETHERTYPE_ARP)
        && std::equal(std::begin(packet_ptr->eth_hdr.ether_dhost), std::begin(packet_ptr->eth_hdr.ether_dhost), std::begin(test_pkt.eth_hdr.ether_shost))){
			/* 5. if right, make attack packet and send */
			memcpy(storage, packet_ptr->eth_hdr.ether_shost, ETHER_ADDR_LEN);
			break;
		}
	}
}

// 패킷 보내기 + 동적할당한 메모리 해제 //
void PKT::send_packet(){
    pcap_res = pcap_sendpacket(pcap_handler, pkt_ptr, pktsize);
	if (pcap_res != 0) {
		fprintf(stderr, "pcap_sendpacket return %d error=%s\n", pcap_res, pcap_geterr(pcap_handler));
		exit(-1);
	}
    switch(pkttype){
        case TCP:
            delete tcp; break;
        case UDP:
            delete udp; break;
        case ICMP:
            delete icmp; break;
        default:
            break;
    }
}

void PKT::make_packet(mac_t* target_mac, ip_t target_ip, int pkttype, int flagtype, int datalen){
    this->pkttype = pkttype;
    
    switch(pkttype){
        case TCP:
            tcp = new(ETHIPTCP);
            memset(tcp, 0, sizeof(ETHIPTCP));
            make_common_part(target_mac, target_ip, (ETHIPHDR*)tcp);
            make_tcp_packet(tcp, flagtype, datalen);
            break;

        /*
        case UDP:
            udp = new(ETHIPUDP);
            memset(udp, 0, sizeof(ETHIPUDP));
            make_common_part(target_ip, (ETHIPHDR*)udp);
            make_udp_packet();
            pkt_ptr = (const uint8_t*)udp;
            break;

        case ICMP:
            icmp = new(ETHIPICMP);
            memset(udp, 0, sizeof(ETHIPICMP));
            make_common_part(target_ip, (ETHIPHDR*)icmp);
            make_icmp_packet();
            pkt_ptr = (const uint8_t*)icmp;
            break;
        */
        default:
            printf("no such packet type\n");
            exit(-1);
    }
}

// 패킷 공통부분 채우기
void PKT::make_common_part(mac_t* target_mac, ip_t target_ip, ETHIPHDR* common){

    // decide eth source, type
    memcpy(common->eth_hdr.ether_dhost, target_mac, ETHER_ADDR_LEN);
    fill_rand(common->eth_hdr.ether_shost, ETHER_ADDR_LEN);
    common->eth_hdr.ether_type = htons(ETHERTYPE_IP);

    // decide common ip header part
    common->ip_hdr.ip_v = 4;                                 
    common->ip_hdr.ip_hl = 5;                                
    common->ip_hdr.ip_tos = IPTOS_LOWDELAY;                  
    fill_rand((uint8_t*)&(common->ip_hdr.ip_id), sizeof(uint16_t));   
    common->ip_hdr.ip_off = htons(IP_DF);                           
    fill_rand((uint8_t*)&common->ip_hdr.ip_ttl, sizeof(uint8_t));    
    fill_rand((uint8_t*)&common->ip_hdr.ip_src.s_addr, sizeof(in_addr_t));
    common->ip_hdr.ip_dst.s_addr = target_ip;
}

// tcp 헤더 채우기
void PKT::make_tcp_packet(ETHIPTCP* tcp_ptr, int flagtype, int datalen){

    /* fill ip protocol as tcp */
    tcp_ptr->ip_hdr.ip_p = IPPROTO_TCP;   
    
    /* fill tcp header */
    fill_rand((uint8_t*)&tcp_ptr->tcp_hdr.th_sport, sizeof(uint16_t));
    tcp_ptr->tcp_hdr.th_dport = htons(80);
    fill_rand((uint8_t*)&tcp_ptr->tcp_hdr.th_seq, sizeof(uint32_t));
    fill_rand((uint8_t*)&tcp_ptr->tcp_hdr.th_ack, sizeof(uint32_t));
    tcp_ptr->tcp_hdr.th_off = 5;
    fill_rand((uint8_t*)&tcp_ptr->tcp_hdr.th_win, sizeof(uint16_t));    //60000~65535되도록 수정필요
    switch(flagtype){
        case SYN:
            tcp_ptr->tcp_hdr.th_flags = TH_SYN;
            break;
        case SYN_ACK:
            tcp_ptr->tcp_hdr.th_flags = TH_SYN | TH_ACK;
            break;
        case ACK:
            tcp_ptr->tcp_hdr.th_flags = TH_ACK;
            break;
    }

    // filling final part
    tcp_ptr->ip_hdr.ip_len = htons(LIBNET_IPV4_H + tcp_ptr->tcp_hdr.th_off * 4 + datalen);
    tcp_ptr->ip_hdr.ip_sum = Checksum((uint16_t*)(&tcp_ptr->ip_hdr), tcp_ptr->ip_hdr.ip_len);
    tcp_ptr->tcp_hdr.th_sum = Checksum((uint16_t*)(&tcp_ptr->tcp_hdr), tcp_ptr->tcp_hdr.th_off);

    pktsize = sizeof(ETHIPTCP) + datalen;
    pkt_ptr = (const uint8_t*)tcp_ptr;
}

void PKT::make_udp_packet(){

}

void PKT::make_icmp_packet(){

}

void PKT::make_specific_packet(
    mac_t* target_mac,
    uint16_t id,
    uint32_t sender_ip, 
    uint32_t target_ip, 
    uint16_t src_port, 
    uint16_t dst_port,
    uint32_t seq,
    uint32_t ack,
    uint16_t window,
    int pkttype, 
    int flagtype, 
    int datalen
    )
{
    this->pkttype = pkttype;
    
    tcp = new(ETHIPTCP);
    memset(tcp, 0, sizeof(ETHIPTCP));
    make_common_part(target_mac, target_ip, (ETHIPHDR*)tcp);
    make_tcp_packet(tcp, flagtype, datalen);


    // 몇 가지 세부 지정

    // IP 재지정
    tcp->ip_hdr.ip_src.s_addr = htonl(sender_ip);

    tcp->tcp_hdr.th_sport = htons(src_port);
    tcp->tcp_hdr.th_dport = htons(dst_port);
    tcp->tcp_hdr.th_seq = htonl(seq);
    tcp->tcp_hdr.th_ack = htonl(ack);
    tcp->tcp_hdr.th_win = htons(window);

    switch(flagtype){
        case SYN:
            tcp->tcp_hdr.th_ack = 0;  // wireshark에서 syn 일 땐 ack는 0이라서
            tcp->tcp_hdr.th_flags = TH_SYN;
            break;
        case SYN_ACK:
            tcp->tcp_hdr.th_flags = TH_SYN | TH_ACK;
            break;
        case ACK:
            tcp->tcp_hdr.th_flags = TH_ACK;
            break;
    }

    // filling final part
    tcp->ip_hdr.ip_len = htons(LIBNET_IPV4_H + tcp->tcp_hdr.th_off * 4 + datalen);
    tcp->ip_hdr.ip_sum = Checksum((uint16_t*)(&tcp->ip_hdr), tcp->ip_hdr.ip_len);
    tcp->tcp_hdr.th_sum = Checksum((uint16_t*)(&tcp->tcp_hdr), tcp->tcp_hdr.th_off);

    this->pktsize = sizeof(ETHIPTCP) + datalen;
    this->pkt_ptr = (const uint8_t*)tcp;
}