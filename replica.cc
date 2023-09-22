/*
 * replication.cc
 *
 *  Created on: 1 ago 2023
 *      Authors: Elisa Composta, Filippo Del Nero, Simone Scevaroli
 */

#import <omnetpp.h>
#include <utility>
#include "global.h"
#import "message_m.h"
#include <iostream>
#include <fstream>
using namespace omnetpp;


class Replica: public cSimpleModule{
private:
    std::map<int, std::pair<std::string, int>> data;    // key - {value, version}
    std::map<int, int> locking_map;     // resource - client (locking)
    std::map<int, int> reservation;     // new resource - client (reserving)
    std::map<int, int> accesses;        // key - accesses (count the accesses to a specific resource)
    bool plot_enabled;
    char logfile[50];
public:
    Replica();
    ~Replica();
protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    virtual std::pair<std::string, int> get(int key);
    virtual bool put(int key, std::string value, int senderId, int newVersion);
    virtual void update(int key, std::string value, int version);

    virtual bool lockResource(int key, int clientGate);
    virtual bool isReserved(int key, int clientGate);

    virtual int getVersion(int key);
    virtual int getClientGate(cGate *gt);

    virtual void printData();
    virtual void printMaps();
    virtual void printAccesses();
};

Define_Module(Replica);


Replica::Replica(){
    int locked = -1;  // default lock status: unlocked
    int version_number = 0;

    data[0] = {"Amsterdam", version_number};
    data[1] = {"Berlin", version_number};
    data[2] = {"Copenhagen", version_number};
    data[3] = {"Dublin", version_number};
    data[4] = {"Edinburgh", version_number};

    locking_map[0] = locked;
    locking_map[1] = locked;
    locking_map[2] = locked;
    locking_map[3] = locked;
    locking_map[4] = locked;

    accesses[0] = 0;
    accesses[1] = 0;
    accesses[2] = 0;
    accesses[3] = 0;
    accesses[4] = 0;
}


Replica::~Replica(){
    data.clear();
}


void Replica::initialize(){
    plot_enabled = par("plot_enabled");
    char logfile_name[50] = "./results/replica/logs/log_";
    strcat(logfile_name, getName());
    char ext[10] = ".txt";
    strcat(logfile_name, ext);
    sprintf(logfile, "%s", logfile_name);
    std::remove(logfile_name);
    printData();
}


void Replica::handleMessage(cMessage *msg){
    Message *ms = dynamic_cast<Message*> (msg);
    Message *msgToSend;

    int key = ms->getKey();
    int clientGate = getClientGate(ms->getArrivalGate());

    std::ofstream f(logfile, std::ios::app);
    if (f.is_open()) {
        f <<"Sender: client"<< clientGate <<std::endl<<"Message: ";
    }else{
        std::cerr << "Error opening the log";
    }

    // GET
    if(ms->getKind() == GET){
        bubble("Get");
        std::pair<std::string, int> get_pair = get(key);
        f << "Get("<< key <<")" <<std::endl;
        if(!strcmp(get_pair.first.c_str(), "refused")){     //data locked
            msgToSend = new Message("refused");
            msgToSend->setValue("refused");
            msgToSend->setVersion(get_pair.second);
            msgToSend->setStatus(REFUSED);
            f <<"The GET is refused" << std::endl;
        }else if(!strcmp(get_pair.first.c_str(), "NULL")){  //data not retrieved
            msgToSend = new Message("value");
            msgToSend->setValue("NULL");
            msgToSend->setVersion(get_pair.second);
            msgToSend->setStatus(SUCCESS);
            f <<"The GET read NULL" << std::endl;
        }else{
            msgToSend = new Message("value");
            msgToSend->setValue(get_pair.first.c_str());
            msgToSend->setVersion(get_pair.second);
            msgToSend->setStatus(SUCCESS);
            f <<"Read the value: "<< msgToSend->getValue()<< " - version: " << msgToSend->getVersion() <<std::endl;
        }
    // LOCK
    } else if(ms->getKind() == LOCK){
        bubble("LockRequest");
        f <<"Lock("<<key<<")"<<std::endl;
        if(lockResource(key, clientGate)) {
            msgToSend = new Message("lock");
            msgToSend->setValue(ms->getValue());
            msgToSend->setVersion(getVersion(key));
            msgToSend->setStatus(SUCCESS);
            f <<"The LOCK is granted with version: " << msgToSend->getVersion() <<std::endl<<std::endl;
            printMaps();
        } else {
            msgToSend = new Message("refused");
            msgToSend->setValue("refused");
            msgToSend->setStatus(REFUSED);
            msgToSend->setVersion(-1);
            f <<"The LOCK is refused"<<std::endl<<std::endl;
        }
    // PUT
    } else if(ms->getKind() == PUT) {
        bubble("Put");
        f <<"Put("<<key<<","<<ms->getValue()<<") - version: "<<ms->getVersion() <<std::endl;
        if(put(key, ms->getValue(), clientGate, ms->getVersion())){
            f <<"The PUT is done"<<std::endl;
        }else{
            f <<"The PUT didn't write (but could have free the resource)" << std::endl;
        }
        printData();
        printMaps();

    // UPDATE
    } else if(ms->getKind() == UPDATE) {
        bubble("Update");
        update(key, ms->getValue(), ms->getVersion());
        f <<"Trying to update: {"<<key<<", "<<ms->getValue()<<", "<<ms->getVersion()<<"}" << std::endl;
        printData();
    }else if(ms->getKind()==UNLOCK){
        bubble("Unlock");
        f <<"Unlock("<<key<<")"<<std::endl;
        if(locking_map.find(key)!=locking_map.end() && locking_map[key]==clientGate){
            if(reservation.find(key)!=reservation.end() && reservation[key]==clientGate){
                reservation.erase(key);
            }
            locking_map[key] = -1;
        }
        printMaps();
    }else {
        msgToSend = new Message("Not defined");
    }

    /* GET, LOCK, undefined message: send response message */
    if(ms->getKind()!=UPDATE && ms->getKind()!=PUT && ms->getKind() != UNLOCK){
        msgToSend->setKey(key);
        msgToSend->setKind(ms->getKind());
        msgToSend->setSeq(ms->getSeq());
        cGate *outGate = gate("gate$o", clientGate);
        send(msgToSend, outGate);
    }

    delete(msg);
    f <<"----------------"<<std::endl<<std::endl;
    f.close();
}


void Replica::finish(){
    printAccesses();
    printData();
}


/* GET */
std::pair<std::string, int> Replica::get(int key){
    //data exists
    if(locking_map.count(key) != 0) {
        //and is unlocked
        if(locking_map[key] == -1){
            accesses[key]++;
            return data[key];
        }else{
            return {"refused", -1};
        }
    }
    return {"NULL", 1};
}


/* PUT */
bool Replica::put(int key, std::string value, int senderGate, int newVersion){
    //if the data exists and the sender owns the lock
    if(locking_map.count(key) != 0 && locking_map[key] == senderGate){
        //put with more recent version -> modify the data and unlock resource
        if(data[key].second < newVersion){
            data[key] = {value, newVersion};
            locking_map[key] = -1;
            return true;
        //put old version -> only unlock
        }else{
            locking_map[key] = -1;
            return false;
        }
    //new data
    }else if(reservation.find(key) != reservation.end() && reservation[key] == senderGate){
        data[key] = {value, newVersion};
        accesses[key]=1;
        reservation.erase(key);
        locking_map[key] = -1;
        return true;
    }
    return false;
}


/* UPDATE */
void Replica::update(int key, std::string value, int version) {
    //resource exists, is unlocked, old version
    if(locking_map.count(key) != 0 && locking_map[key] == -1 && data[key].second < version){
        accesses[key]++;
        data[key] = {value, version};
    }
}


/* Return true if client successfully locked the resource, false if it was already locked */
bool Replica::lockResource(int key, int clientGate){
    //data exists
    if(locking_map.find(key)!=locking_map.end()){
        //resource not already locked
        if(locking_map[key]==-1){
            accesses[key]++;
            locking_map[key] = clientGate;
            return true;
        }else{
            return false; //data is locked
        }
    //new data
    }else{
        return !isReserved(key, clientGate);
    }
}


/* Returns false if client successfully reserved the new data, true if the resource was already reserved */
bool Replica::isReserved(int key, int clientGate){
    //already reserved
    if(reservation.find(key)!=reservation.end()){
        return true;
    }else{
        reservation[key] = clientGate;
        return false;
    }
}


/* Get version of the resource identified by key */
int Replica::getVersion(int key){
    if(data.find(key)!=data.end()){
        return data[key].second;
    }
    return -1;
}


/* Get index of client gate */
int Replica::getClientGate(cGate *gt) {
    for(int i = 0; i < gateSize("gate$i"); i++){
        if(gt == gate("gate$i", i)){
            return i;
        }
    }
    return -1;
}


/********** PRINT **********/

/* Log content of replicated database */
void Replica::printData(){
    std::ofstream f(logfile, std::ios::app);
    if (f.is_open()) {
        f << "Database:"<<std::endl;
        for (const auto& el : data ) {
            f <<"\t"<< el.first << " " << el.second.first << " " << el.second.second << std::endl;
        }
        f << std::endl;
        f.close();
    }else{
        std::cerr <<"Error opening the file"<<std::endl;
    }
}


/* Log maps of locked and reserved resources */
void Replica::printMaps(){
    std::ofstream f(logfile, std::ios::app);
    if (f.is_open())
    {
        //Locking map
        f <<"Locking map:"<<std::endl;
        for (const auto& el : locking_map ) {
            if(el.second!=-1){
                f <<"\t"<< "Resource "<<el.first << " locked by client" << el.second << std::endl;
            }
        }
        f << std::endl;

        //Reservation
        if(reservation.size()!=0){
            f << "Reservations:"<<std::endl;
            for (const auto& el : reservation ) {
                f <<"\t"<< "Key " <<el.first << " reserved by " << el.second << std::endl;
            }
            f << std::endl;
        }

        f.close();
    }else{
        std::cerr <<"Error opening the file"<<std::endl;
    }
}


/* Store in a .csv the accesses per resource and plot data */
void Replica::printAccesses(){
    char file[50] = "./results/replica/accesses/replica_";
    strcat(file, getName());
    char ext[10] = ".csv";
    strcat(file, ext);
    std::remove(file);
    std::ofstream f(file, std::ios::app);
    if (f.is_open()) {
        f<<"resource"<<";"<<"accesses"<<std::endl;
        for (const auto& el : accesses ) {
            f << el.first << ";" << el.second << std::endl;
        }
    }else{
        std::cerr <<"Error opening the file";
    }
    f.close();

    if(plot_enabled){
        char command[50] = "python plot_res_accesses.py ";
        strcat(command, getName());
        if(!std::system(command)){
            std::cout << "File res_accesses_" << getName() << ".png created in results/plots" << std::endl;
        } else {
            std::cerr << "Error in creating res_accesses_" << getName() << ".png" << std::endl;
        }
    }
}

