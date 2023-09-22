package project;

simple Replica
{
    parameters:
        @display("i=block/buffer2");
        int n_client = 4;
        bool plot_enabled = default(false);
    gates:
        inout gate[n_client];
}


simple Client
{
    parameters:
        @display("i=old/comp");
        int n_replica = 5;
        int read_quorum = default(3);
        int write_quorum = default(4);
        bool plot_enabled = default(false);
    gates:
        inout gate[n_replica];
}


network Replication
{
    @display("bgb=680,440");
    types:
        channel C50 extends ned.DatarateChannel
        {
            delay = 50ms;
            datarate = 100Mbps;
        }
        channel C500 extends ned.DatarateChannel
        {
            delay = 500ms;
            datarate = 100Mbps;
        }
        channel C5000 extends ned.DatarateChannel
        {
            delay = 5000ms;
            datarate = 100Mbps;
        }

        channel CUniform extends ned.DatarateChannel
        {
            delay = uniform(100ms,500ms);
            datarate = 100Mbps;
        }
    submodules:
        r0: Replica {
            @display("i=,gold;p=80,80");
        }
        r1: Replica {
            @display("i=,gold;p=200,80");
        }
        r2: Replica {
            @display("i=,gold;p=320,80");
        }
        r3: Replica {
            @display("i=,gold;p=440,80");
        }
        r4: Replica {
            @display("i=,gold;p=560,80");
        }
        client0: Client {
            @display("i=,cyan;p=140,320");
        }
        client1: Client {
            @display("i=,cyan;p=260,320");
        }
        client2: Client {
            @display("i=,cyan;p=380,320");
        }
        client3: Client {
            @display("i=,cyan;p=500,320");
        }

    connections:
        client0.gate[0] <--> C50 <--> r0.gate[0];
        client0.gate[1] <--> C50 <--> r1.gate[0];
        client0.gate[2] <--> C50 <--> r2.gate[0];
        client0.gate[3] <--> C500 <--> r3.gate[0];
        client0.gate[4] <--> C500 <--> r4.gate[0];

        client1.gate[0] <--> C50 <--> r0.gate[1];
        client1.gate[1] <--> C50 <--> r1.gate[1];
        client1.gate[2] <--> C50 <--> r2.gate[1];
        client1.gate[3] <--> C50 <--> r3.gate[1];
        client1.gate[4] <--> C50 <--> r4.gate[1];

        client2.gate[0] <--> C50 <--> r0.gate[2];
        client2.gate[1] <--> C50 <--> r1.gate[2];
        client2.gate[2] <--> C50 <--> r2.gate[2];
        client2.gate[3] <--> C500 <--> r3.gate[2];
        client2.gate[4] <--> C500 <--> r4.gate[2];

        client3.gate[0] <--> C50 <--> r0.gate[3];
        client3.gate[1] <--> C50 <--> r1.gate[3];
        client3.gate[2] <--> C50 <--> r2.gate[3];
        client3.gate[3] <--> C50 <--> r3.gate[3];
        client3.gate[4] <--> C50 <--> r4.gate[3];

}
