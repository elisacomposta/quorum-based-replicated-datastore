/*
 * client.cc
 *
 *  Created on: 1 ago 2023
 *      Authors: Elisa Composta, Filippo Del Nero, Simone Scevaroli
 */

#import <omnetpp.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "global.h"
#import "message_m.h"
using namespace omnetpp;


class Client: public cSimpleModule{
private:
    int read_quorum;
    int write_quorum;
    bool plot_enabled;

    //collect responses from replica
    std::map<int, std::map<std::pair<std::string, int>, std::unordered_set<int>>> res;
    //seq_number |-> [(value1, version1) -> replica1, replica2, replica5]
    //           |-> [(value2, version2) -> replica3, replica4]

    std::map<int, std::unordered_set<int>> pendingLock;  // key - replica1, replica2, replica3
    std::map<int, int> reachedQuorum; //seqNum - version quorum (LOCK and GET only)

    char logfile[50];

    int seqNumber = 0;
    int putReqNum = 0;
    int getReqNum = 0;
    int RejectedNum = 0;

    cOutVector avgTimeToPut; //data collection
    std::map<int, simtime_t> starting_time_put; //data collection

    bool is_example7 = false;

protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    virtual void sendGet(int key);
    virtual void sendGetDelayed(int key, simtime_t delay);

    virtual void sendPut(int key, char * value);
    virtual void sendPutDelayed(int key, char * value, simtime_t delay);
    virtual void putLocked(int key, char * value, int version, int seq);

    virtual void sendUpdate(int key, char *value, int version, int replica_id);
    virtual void sendUnlock(int key);

    virtual void sendReplicas(Message *msg);
    virtual void sendSingleReplica(Message *msg, int replica_id);

    virtual int getReplicaGate(cGate *gt);
    virtual bool isQuorum(Message *msg);
    virtual int countResponses(int seq);
    virtual void checkStaleCopies(int key, int msg_seq);

    virtual void printRes();
    virtual void printLock();
    virtual void printReachedQuorum();
    virtual void printOperations();
    virtual void printLog(Message *msg, bool toSelf, bool quorum);

    virtual void example1();
    virtual void example2();
    virtual void example3();
    virtual void example4();
    virtual void example5();
    virtual void example6();
    virtual void example7();

    virtual void setup_example7();
    virtual void update_channel_delays();
};

Define_Module(Client);


void Client::initialize(){
    //assign parameters as specified in the omnetpp.ini file
    read_quorum = par("read_quorum");
    write_quorum = par("write_quorum");
    plot_enabled = par("plot_enabled");

    //initialize client logs
    char logfile_name[50] = "./results/client/log_";
    strcat(logfile_name, getName());
    char ext[10] = ".txt";
    strcat(logfile_name, ext);
    sprintf(logfile, "%s", logfile_name);
    std::remove(logfile_name);
    std::ofstream f1(logfile_name, std::ios::app);
    f1.close();

    //track values
    WATCH(seqNumber);
    WATCH(putReqNum);
    WATCH(getReqNum);
    WATCH(RejectedNum);

    //run simulation
    example7();
}


void Client::handleMessage(cMessage *msg){
    Message *ms = dynamic_cast<Message*> (msg->dup());
    int seq = ms->getSeq();
    bool toSelf = false;
    bool justQuorum = false;

    /* send scheduled message */
    if(msg->isSelfMessage()){
        toSelf = true;
        if(ms->getReceiver()==-1){
            sendReplicas(ms);
        }else{
            sendSingleReplica(ms, ms->getReceiver());
        }
    /* message from replicas */
    } else {
        int replicaId = getReplicaGate(ms->getArrivalGate());
        res[seq][{ms->getValue(), ms->getVersion()}].insert(replicaId);

        if(ms->getKind()==LOCK && ms->getStatus()==SUCCESS ){
            pendingLock[ms->getKey()].insert(replicaId);
        }

        /*just reached quorum*/
        if(isQuorum(ms) && reachedQuorum[seq]==-1){
            justQuorum = true;
            if(ms->getStatus() == SUCCESS) {
                /* LOCK quorum */
                if(ms->getKind()==LOCK){
                    bubble("Lock granted");
                    char s[20];
                    sprintf(s, "%s", ms->getValue());
                    int version = ms->getVersion();
                    reachedQuorum[seq]=++version;
                    EV<<getName()<<": QUORUM on LOCK. Performing PUT(" << ms->getKey()<<", "<< ms->getValue()<<", "<< version << ")"<< std::endl;
                    putLocked(ms->getKey(), s, version, seq+1);
                    simtime_t time = simTime() - starting_time_put[seq];
                    avgTimeToPut.record(time); //data collection
                /* GET quorum */
                } else if(ms->getKind()==GET){
                    reachedQuorum[seq]=ms->getVersion();
                    bubble(ms->getValue());
                    if(strcmp(ms->getValue(), "NULL")!=0){
                        EV<<getName()<<": QUORUM on GET(" <<ms->getKey()<<"). READ: (" << ms->getValue()<<", v"<<ms->getVersion()<<")"<<std::endl;
                    } else {
                        EV<<getName()<<": QUORUM on GET(" <<ms->getKey()<<"). READ: " << ms->getValue()<<std::endl;
                    }
                }
            } else if(ms->getStatus() == REFUSED){
                if(ms->getKind()==LOCK){
                    bubble("Lock refused");
                    EV<<getName()<<": LOCK failed, resource " << ms->getKey() << " already locked."<<std::endl;
                } else if(ms->getKind()==GET){
                    bubble("Get refused");
                    reachedQuorum[seq]=-2;
                    EV<<getName()<<": QUORUM on GET("<<ms->getKey()<<"). REFUSED"<<std::endl;
                }
                RejectedNum++;
            }
        }else if(ms->getKind()==LOCK && ms->getStatus()==SUCCESS && reachedQuorum[ms->getSeq()]!=-1){
            char s[20];
            sprintf(s, "%s", ms->getValue());
            int version = reachedQuorum[seq];
            putLocked(ms->getKey(), s, version , seq+1);
        }

        /*all answers received*/
        if(countResponses(seq)==gateSize("gate$i")){
ì            if(ms->getKind()==GET){
                if(reachedQuorum[seq]!=-1){
                    checkStaleCopies(ms->getKey(), seq);
                //not quorum
                } else if(reachedQuorum[seq]!=-2) {
                    bubble("Get refused");
                    RejectedNum++;
                    EV<<getName()<<": QUORUM on GET("<<ms->getKey()<<"). REFUSED"<<std::endl;
                }
            }
            /*first lock or not reached lock quorum*/
            if(ms->getKind()==LOCK && (reachedQuorum.find(ms->getKey())==reachedQuorum.end() || reachedQuorum[seq]==-1)){
                sendUnlock(ms->getKey());
            }
            res.erase(seq);
            reachedQuorum.erase(seq);
        }
    }
    printLog(ms, toSelf, justQuorum);
    delete(ms);
    delete(msg);
}


void Client::finish(){
    printOperations();

}


void Client::sendGet(int key){
    sendGetDelayed(key, 0);
}


void Client::sendGetDelayed(int key, simtime_t delay){
    Message *msg = new Message("get");
    getReqNum++;
    reachedQuorum[seqNumber]=-1;
    msg->setKind(GET);
    msg->setSeq(seqNumber++);
    msg->setKey(key);
    scheduleAt(simTime()+delay, msg);
    EV<<getName()<<": GET(" << msg->getKey() << ")" << std::endl;
}


void Client::sendPut(int key, char *value){
    sendPutDelayed(key, value, 0);
}


void Client::sendPutDelayed(int key, char * value, simtime_t delay){    // request lock on key
    Message *msg = new Message("lock");
    putReqNum++;
    reachedQuorum[seqNumber]=-1;
    msg->setKind(LOCK);
    msg->setSeq(seqNumber++);
    msg->setKey(key);
    msg->setValue(value);
    starting_time_put[msg->getSeq()] = simTime() + delay; //data collection
    scheduleAt(simTime()+delay, msg);
}


void Client::putLocked(int key, char * value, int version, int seq){
    for(auto replicaId: pendingLock[key]){
        Message *msg = new Message("put");
        msg->setKind(PUT);
        msg->setSeq(seq);
        msg->setKey(key);
        msg->setValue(value);
        msg->setReceiver(replicaId);
        msg->setVersion(version);
        scheduleAt(simTime(), msg);
    }
    pendingLock.erase(key);
}


void Client::sendUpdate(int key, char *value, int version, int replica_id){
    Message *msg = new Message("update");
    msg->setKind(UPDATE);
    msg->setSeq(seqNumber++);
    msg->setKey(key);
    msg->setValue(value);
    msg->setVersion(version);
    msg->setReceiver(replica_id);
    scheduleAt(simTime(), msg);
}


void Client::sendUnlock(int key){
    for(auto replicaId: pendingLock[key]){
        Message *msg = new Message("unlock");
        msg->setKind(UNLOCK);
        msg->setSeq(seqNumber+1);
        msg->setKey(key);
        msg->setReceiver(replicaId);
        scheduleAt(simTime(), msg);
    }
    pendingLock.erase(key);
}


/* Send msg to all replicas */
void Client::sendReplicas(Message *msg) {
    /* NON SEQ CONSISTENCY ONLY */
    if(msg->getSeq() != 0 && is_example7) { update_channel_delays(); }

    /* send to all replicas */
    for(int i = 0; i < gateSize("gate$o"); i++){
        Message *newMsg = msg->dup();
        send(newMsg, gate("gate$o", i));
    }
}


/* Send msg to replica with gate index replica_id */
void Client::sendSingleReplica(Message *msg, int replica_id) {
    Message *newMsg = msg->dup();
    send(newMsg, gate("gate$o", replica_id));
}


/* Return index of replica gate */
int Client::getReplicaGate(cGate *gt) {
    for(int i = 0; i < gateSize("gate$i"); i++){
        if(gt == gate("gate$i", i)){
            return i;
        }
    }
    return -1;
}


/* Returns if msg reached quorum */
bool Client::isQuorum(Message *msg) {
    int count = res[msg->getSeq()][{msg->getValue(), msg->getVersion()}].size();
    int kind = msg->getKind();
    int status = msg->getStatus();
    if(kind == GET){
        return read_quorum == count;
    }else if(kind == PUT || kind == LOCK || status == REFUSED){
        return write_quorum == count;
    }
    return false;
}


/* Count how many responses an operation received */
int Client::countResponses(int seq){
    int count = 0;
    for(auto row: res[seq]){
        count += row.second.size();
    }
    return count;
}


/* Update replicas not up-to-date */
void Client::checkStaleCopies(int key, int msg_seq) {
    int quorum_version = reachedQuorum[msg_seq];
    std::string quorum_value = "";

    /* find latest version and latest value */
    for(auto row : res[msg_seq]) {
        if(row.first.second == quorum_version) {
            quorum_value = row.first.first;
            break;
        }
    }

    char* quorum_value_str = new char[20];
    strcpy(quorum_value_str, quorum_value.c_str());

    /*check if any replica has a stale value and send update*/
    for(auto row : res[msg_seq]) {
        if(row.first.second < quorum_version && strcmp(row.first.first.c_str(),"refused")) {
            for(auto replica : row.second) { //iterate over non updated replicas
                sendUpdate(key, quorum_value_str, quorum_version, replica);
            }
        }
    }
}




/********** PRINT **********/

void Client::printRes() {
    std::ofstream f(logfile, std::ios::app);
    f << "Res table:"<<std::endl;
    for (const auto& row : res ) {
        f << row.first << ": "<<std::endl;
        for(auto reply: row.second){
            f << "["<<reply.first.first << ", " << reply.first.second<< "] -> ";
            for(auto replica : reply.second){
                f << replica <<", ";
            }
            f << std::endl;
        }
        f << std::endl;
    }
    f << "-------------" << std::endl;
    f.close();
}


void Client::printLock(){
    std::ofstream f(logfile, std::ios::app);
    f << "Pending locks:"<<std::endl;
    for (const auto& el : pendingLock ) {
        f << el.first << ": ";
        for(auto repl: el.second){
            f << repl << " ";
        }
        f << std::endl;
    }
    f << "-------------" << std::endl;
    f.close();
}


void Client::printReachedQuorum(){
    std::ofstream f(logfile, std::ios::app);
    f << "Reached quorum:"<<std::endl;
    for (const auto& el : reachedQuorum ) {
        f << el.first << ": "<<el.second<< std::endl;
    }
    f << "-------------" << std::endl;
    f.close();
}


void Client::printOperations(){
    const char* file = "./results/client/client_operations.csv";

    if(!strcmp(getName(), "client0")){
        std::remove(file);
        std::ofstream f1(file, std::ios::app);
        f1<<"client"<<";"<<"get"<<";"<<"put"<<";"<<"rejected"<<std::endl;
        f1.close();
    }
    std::ofstream f(file, std::ios::app);

    if (f.is_open()) {
        f<<getName()<<";";
        f<< getReqNum<<";";
        f<< putReqNum<<";";
        f<< RejectedNum<<std::endl;
    }else{
        std::cerr <<"Error opening the file";
    }
    f.close();
    if(!strcmp(getName(), "client3") && plot_enabled){
        if(!std::system("python plot_client_operations.py")){
            std::cout << "File client_operations.png created in results/plots" << std::endl;
        } else {
            std::cerr << "Error in creating client_operations.png" << std::endl;
        }
    }
}


void Client::printLog(Message *msg, bool toSelf, bool quorum) {
    std::ofstream log(logfile, std::ios::app);
    if(toSelf) {
        if(msg->getKind() == GET) {
            if(msg->getReceiver() == -1)
                log << "Sent GET to ALL.       key: " << msg->getKey() << std::endl;
            else
                log << "Sent GET to R" << msg->getReceiver() <<". key: " << msg->getKey() << std::endl;
        } else if(msg->getKind() == PUT) {
            if(msg->getReceiver() == -1)
                log << "Sent PUT to ALL. key: " << msg->getKey() << " | value: " << msg->getValue() << " | version: " << msg->getVersion() << std::endl;
            else
                log << "Sent PUT to R" << msg->getReceiver() <<".        key: " << msg->getKey() << " | value: " << msg->getValue() << " | version: " << msg->getVersion() << std::endl;
        } else if(msg->getKind() == LOCK) {
            if(msg->getReceiver() == -1)
                log << "Sent LOCK to ALL.      key: " << msg->getKey() << std::endl;
            else
                log << "Sent LOCK to R" << msg->getReceiver() <<". key: " << msg->getKey() << std::endl;
        } else if(msg->getKind() == UPDATE) {
            log << "Sent UPDATE to R" << msg->getReceiver() <<".     key: " << msg->getKey() << " | value: " << msg->getValue() << " | version: " << msg->getVersion() << std::endl;
        } else if(msg->getKind() == UNLOCK) {
            log << "Sent UNLOCK to R" << msg->getReceiver() <<".     key: " << msg->getKey() << std::endl;
        }
    } else {
        int replicaId = getReplicaGate(msg->getArrivalGate());
        if(msg->getKind() == GET) {
            if(msg->getStatus() == REFUSED) {
                log << "Received GET from R" << replicaId <<".  key: " << msg->getKey() << ". Status: REFUSED " << std::endl;
            } else {
                if(strcmp(msg->getValue(), "NULL")!=0){
                    log << "Received GET from R" << replicaId <<".  key: " << msg->getKey() << " | value: " << msg->getValue() << " | version: " << msg->getVersion() << std::endl;
                }else{
                    log << "Received GET from R" << replicaId <<".  key: " << msg->getKey() << " | value: " << msg->getValue() << std::endl;
                }
            }
        } else if(msg->getKind() == PUT) {
            if(msg->getStatus() == REFUSED) {
                log << "Received PUT from R" << replicaId <<".  key: " << msg->getKey() << ". Status: REFUSED " << std::endl;
            } else {
                log << "Received PUT from R" << replicaId <<".  key: " << msg->getKey() << " | value: " << msg->getValue() << " | version: " << msg->getVersion() << std::endl;
            }
        } else if(msg->getKind() == LOCK) {
            if(msg->getStatus() == REFUSED) {
                log << "Received LOCK from R" << replicaId <<". key: " << msg->getKey() << ". Status: REFUSED " << std::endl;
            } else {
                log << "Received LOCK from R" << replicaId <<". key: " << msg->getKey() << " | version: " << msg->getVersion() << std::endl;
            }
        }
    }

    if(quorum){
        if(msg->getKind() == GET){
            log<<"**Reached QUORUM on GET";
            if(msg->getStatus() != REFUSED) {
                if(strcmp(msg->getValue(), "NULL")!=0){
                    log << "("<<msg->getKey()<< "). READ: ("<< msg->getValue()<<", v"<<msg->getVersion()<<")\n";
                }else{
                    log << "("<<msg->getKey()<< "). READ: "<< msg->getValue()<<std::endl;
                }
            } else {
                log << "("<<msg->getKey()<<"). REFUSED\n";
            }
        } else if(msg->getKind() == LOCK){
            //**Reached LOCK quorum. Performing PUT(1, Berlin, v1)
            if(msg->getStatus() != REFUSED) {
                log<<"**Reached QUORUM on LOCK. Performing PUT("<<msg->getKey()<<", "<<msg->getValue()<<", "<<msg->getVersion()+1<<")\n";
            } else {
                log<<"**LOCK failed, resource " << msg->getKey() << " already locked.\n";
            }
        }
    }

    log.close();
}




/********** EXAMPLES **********/

/*
 * Run these examples with the following channel delays (ms):
 *  client0: 50 50 50 500 500
 *  client1: 50 50 50 50 50
 *  client2: 50 50 50 500 500
 *  client3: 50 50 50 50 50
 * and with parameters:
 *  read_quorum = 3
 *  write_quorum = 3
 */


/*
 * UNLOCK EXAMPLE on existing resource
 * client3:  put(1, Milan)
 * client0:  put(1, Venice)
 * client1:  get(1)
 */
void Client::example1(){
    if(!strcmp(getName(), "client3")){
        char value[20] = "Milan";
        sendPut(1, value);
    }
    if(!strcmp(getName(), "client0")){
        char value[20] = "Venice";
        sendPutDelayed(1, value, 0.1);
    }
    if(!strcmp(getName(), "client1")){
        sendGetDelayed(1, 2.100);
    }
}


/*
 * UPDATE EXAMPLE before put arrives
 * client0: put(1, Venice)
 * client3: get(1) + update
 * client1: get(1) -> all up-to-date
 */
void Client::example2(){
    if(!strcmp(getName(), "client0")){
        char value[20] = "Venice";
        sendPut(1, value);
    }
    if(!strcmp(getName(), "client3")){
        sendGetDelayed(1, 0.300);
    }
    if(!strcmp(getName(), "client1")){
        char value[20] = "Milan";
        sendGetDelayed(1, 2.100);
    }
}


/*
 * UPDATE EXAMPLE for partial lock
 * client0: put(1, Atlanta) -> locks r0, r1, r2 (slow channels to r3, r4)
 * client1: put(1, Brighton) -> locks r0, r1, r2 (r3, r4 locked by client0)
 * client2: get(1)         -> update r3, r4
 */
void Client::example3(){
    if(!strcmp(getName(), "client0")){
        char value[20] = "Atlanta";
        sendPut(0, value);
    }
    if(!strcmp(getName(), "client1")){
        char value[20] = "Brighton";
        sendPutDelayed(0, value, 0.500);
    }
    if(!strcmp(getName(), "client2")){
        sendGetDelayed(0, 3);
    }
}


/*
 * UPDATE EXAMPLE new put completed before old update arrives
 * client0: put(1, Venice) -> locks r0, r1, r2 (slow channels to r3, r4)
 * client1: put(1, Kyoto) -> locks r0, r1, r2 (r3, r4 locked by client0)
 * client2: get(1)         -> update r3, r4
 * client3: put(1, Singapore)-> new put before update
 * client1: get(1)
 */
void Client::example4(){
    if(!strcmp(getName(), "client0")){
        char value[20] = "Venice";
        sendPut(1, value);
    }
    if(!strcmp(getName(), "client1")){
        char value[20] = "Kyoto";
        sendPutDelayed(1, value, 0.500);
    }
    if(!strcmp(getName(), "client2")){
        sendGetDelayed(1, 3);
    }
    if(!strcmp(getName(), "client3")){
        char value[20] = "Singapore";
        sendPutDelayed(1, value, 3.500);
    }
    if(!strcmp(getName(), "client1")){
        sendGetDelayed(1, 10);
    }
}


/*
 * SEQUENTIAL CONSISTENCY
 * client1:  put(0, Atlanta)
 * client2:  get(0, 0.5),  get(0, 3.2)
 * client3:  get(0, 1.5),    get(0, 4)
 * client4:  put(0, Brighton, 3)
 */
void Client::example5(){
    if(!strcmp(getName(), "client0")){
        char value[20] = "Atlanta";
        sendPut(0, value);
    }
    if(!strcmp(getName(), "client1")){
        sendGetDelayed(0, 0.5);
        sendGetDelayed(0, 3.2);
    }
    if(!strcmp(getName(), "client2")){
        sendGetDelayed(0, 1.5);
        //sendGetDelayed(0, 4);
    }
    if(!strcmp(getName(), "client3")){
        char value[20] = "Brighton";
        sendPutDelayed(0, value, 3);
    }
}


/*
 * Mixture of get-put requests on a variety of different resources
 */
void Client::example6(){
    //client 0
    if(!strcmp(getName(), "client0")){
        char value[20] = "Venezia";
        sendPutDelayed(1, value, 0.1);
        sendGetDelayed(1, 0.3);
        char value1[20] = "Verona";
        sendPutDelayed(2, value1, 0.5);
        char value2[20] = "Vicenza";
        sendPutDelayed(2, value2, 1);
    }

    //client 1
    if(!strcmp(getName(), "client1")){
        char value[20] = "Como";
        sendPutDelayed(3, value, 0.5);
        char value1[20] = "Cagliari";
        sendPutDelayed(5, value1, 1);
        char value2[20] = "Cosenza";
        sendPutDelayed(3, value2, 1.5);
        char value3[20] = "Catania";
        sendPutDelayed(4, value3, 2);
    }

    //client 2
    if(!strcmp(getName(), "client2")){
        sendGetDelayed(1, 0.2);
        sendGetDelayed(3, 0.4);
        sendGetDelayed(3, 0.7);
        sendGetDelayed(1, 0.8);
    }

    //client 3
    if(!strcmp(getName(), "client3")){
        char value[20] = "Modena";
        sendPutDelayed(2, value, 1);
        sendGetDelayed(2, 1.1);
        char value1[20] = "Mantova";
        sendPutDelayed(7, value1, 1.5);
        char value2[20] = "Milano";
        sendPutDelayed(5, value2, 1.6);
    }
}


/*
 * NO SEQUENTIAL CONSISTENCY
 * parameters:
 *  r_quorum = 1
 *  w_quorum = 2
 * channels: (default C50)
 *      Client1 -> R2, R3, R4 -> C10000
 *      Client2 -> R0, R1, R2, R3, R4 -> C10000
 *      Client3 -> R0, R1, R2, R3, R4 -> C10000
 */
void Client::example7() {
    setup_example7();
    if(!strcmp(getName(), "client1")) {
        char value[20] = "Barcelona";
        sendPutDelayed(0, value, 0);
    }

    if(!strcmp(getName(), "client2")) {
        sendGetDelayed(0, 0.5); //R0 fastest channel
        sendGetDelayed(0, 2);   //R3 fastest channel
    }

    if(!strcmp(getName(), "client3")) {
        sendGetDelayed(0, 0.5); //R3 fastest channel
        sendGetDelayed(0, 2);   //R0 fastest channel
    }
}


void Client::setup_example7() {
    is_example7 = true;

    if(!strcmp(getName(), "client1")) {
        //channel 3, 4
        for(int i=3; i<5; i++){
            cDatarateChannel *channel = check_and_cast<cDatarateChannel *>(gate("gate$o", i)->getChannel());
            channel->setDelay(500);
        }
    }
    if(!strcmp(getName(), "client2")) {
        //channel 1, 2, 3, 4
        for(int i=1; i<5; i++){
            cDatarateChannel *channel = check_and_cast<cDatarateChannel *>(gate("gate$o", i)->getChannel());
            channel->setDelay(500);
        }
    }
    if(!strcmp(getName(), "client3")) {
        //channel 0, 1, 2, 4
        for(int i=0; i<5; i++){
            if(i!=3){
                cDatarateChannel *channel = check_and_cast<cDatarateChannel *>(gate("gate$o", i)->getChannel());
                channel->setDelay(500);
            }
        }
    }
}


/* Update channel delay for non-sequential consistency example */
void Client::update_channel_delays(){
    if(!strcmp(getName(), "client2")) {
         cDatarateChannel *channel = check_and_cast<cDatarateChannel *>(gate("gate$o", 0)->getChannel());
         channel->setDelay(500);

         cDatarateChannel *channel1 = check_and_cast<cDatarateChannel *>(gate("gate$o", 3)->getChannel());
         channel1->setDelay(0.050);
     }
     if(!strcmp(getName(), "client3")) {
         cDatarateChannel *channel2 = check_and_cast<cDatarateChannel *>(gate("gate$o", 3)->getChannel());
         channel2->setDelay(500);

         cDatarateChannel *channel3 = check_and_cast<cDatarateChannel *>(gate("gate$o", 0)->getChannel());
         channel3->setDelay(0.050);
     }
}









