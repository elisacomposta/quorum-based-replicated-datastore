Project for _Distributed Systems_ (_Politecnico di Milano_, 2022-2023)

# Quorum-based replicated datastore
This is the Omnet++ implementation of a distributed key-value datastore that accepts two operations from clients:
- _put ( k, v )_ &emsp; insert/update value _v_ for key _k_;
- _get ( k )_ &emsp; get the value associated with key _k_ (or NULL if the key is not present).

The store is internally replicated across N nodes (processes) and offers sequential consistency using a quorum-based protocol.

## Modules
The modules defined in _replication.ned_ are:
- **replica**
- **client** 

<img src="https://github.com/elisacomposta/quorum-based-replicated-datastore/assets/98694899/31e022c2-1d80-4f54-b914-ac7e5de9fe38" width="45%" height="45%">


## Channels
Each client is connected to each replica through the following channels:
- **C50**: delay = 50ms, datarate = 100Mbps<br>
- **C500**:  delay = 500ms, datarate = 100Mbps

  
The default channel used is _C50_, however both _client0_ and _client2_ have channel of type _C500_ to connect to _R3_ and _R4_.


## Parameters
- **read_quorum**
- **write_quorum**
- **plot_enabled** to allow for automatic data plotting at the end of the simulation

## Assumptions
- reliable processes and links
- no network partitions
- FIFO channels

## Concurrency
Example of **read-write concurrency**:<br>
_client2_	&emsp; put ( 3,  Paris )<br>
_client4_	&emsp; get ( 3 )<br>
 
Example of **write-write concurrency**: <br>
_client1_ &emsp; put ( 7, Venice )<br>
_client4_	&emsp; put ( 7, Milan  ) <br>

To solve the concurrency problem, we implemented _locking_, since it ensures exclusive access to a resource for a write operation (_put_) and is released when the operation is completed.<br>
- **PUT**
  - request lock
  - if lock granted, perform put
  - release lock <br>
- **GET**
  - if resource is unlocked, perform get <br>

## Versioning
A slow put can potentially overwrite a new value with an old one.<br>
For example:<br>
_client0_ &emsp; put ( 3, Paris )<br>
_client1_ &emsp; put ( 3, Singapore )<br>

To avoid this problem, versioning has been implemented: replicas also stores the version of the data.<br>
- **PUT**
  - client must include the version in the message
- **GET**
  - replicas also return the version
 
## Update: read-repair
When a client performs a _get_, reaches the quorum on  the value to read and detects some stale responses, it sends the new value to the replicas that are not up-to-date.<br>
Please note that the replica updates the data only if the value is more recent (i.e. higher version).

## Data collected
Data collected during the simulation are stored in the `results/` folder.<br>
A _vector_ stores the time to lock a resource. Other data collected are stored in the following folders:

- `results/client/`
  - _client_operations.csv_ stores the number of _put_, _get_ and _refused_ operations per client
  - the log files of all clients track the messages sent and received, and the reached quorum
- `results/replica/`
  - _accesses_ folder contains the _.csv_ files storing the number of accesses per resource<br>
  - _logs_ folder contains log files showing the evolution of the database of each replica
- `results/plots/`
  - _client\_operations.png_ is the plot of the number of operations per client
  - _res\_accesses.png_ is the plot of the number of accesses per resource (one plot per replica)


## Plots
It is possible to enable the plotting of collected data on a bar chart by setting the 'plot_enabled' parameter. 
This setting will automatically trigger the execution of the following python files at the end of the simulation:<br>
- **plot_client_operations.py**: number of operations per client
- **plot_res_accesses.py**: number of accesses per resource

<img src="https://github.com/elisacomposta/quorum-based-replicated-datastore/assets/98694899/f59faecc-8c6b-4457-be65-6943d1c99a20" width="45%" height="45%"> <br>
Please note that different replicas can have different number of accesses per resource, since a resource can be locked on a replica for a long time, causing the rejection of other operations.<br>
<img src="https://github.com/elisacomposta/quorum-based-replicated-datastore/assets/98694899/a5a71d49-6b51-40ea-8a17-c474193075fa" width="45%" height="45%">
<img src="https://github.com/elisacomposta/quorum-based-replicated-datastore/assets/98694899/f789abb9-e8b8-46f0-9f90-735cd32cf3ce" width="45%" height="45%">

