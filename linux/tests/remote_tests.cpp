#include "remote_station.h"
#include "remote_test_transport.h"
#include "core/wav.h"
#include <QtTest>
#include <QtEndian>
#include <QJsonArray>

class RemoteTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory_;
    const QString key_="0123456789abcdef0123456789abcdef";
    void configure(remote::Server& server){server.start(0,key_,QHostAddress::LocalHost);server.canTransmit=[]{return QString{};};}
    void pair(remote::Server& server,remote::Client& client,const QString& key={}){
        const auto [shack,laptop]=remoteTestPair();server.acceptConnection(shack);client.connectDevice(laptop,key.isEmpty()?key_:key);
    }
    QString wave(int samples=96000){sstv::Audio audio;audio.sampleRate=48000;audio.samples.resize(size_t(samples),.125f);const auto path=directory_.filePath("tx.wav");sstv::writeWav(path,audio);return path;}
private slots:
    void delayedTransmissionRequestsCannotUseAnotherRadioProfile(){
        remote::Server server;configure(server);QJsonArray items{QJsonObject{{"id","radio-a"}},QJsonObject{{"id","radio-b"}}};QJsonObject state{{"profiles",QJsonObject{{"active","radio-a"},{"items",items}}}};server.setSnapshot(state);int voices=0,transmissions=0;server.beginVoice=[&](remote::Server::Ready ready){++voices;ready({});};server.transmit=[&](const QString&,qint64,qint64){++transmissions;return QString{};};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());
        // Change the server now; the client has not processed that Wi-Fi update.
        state["profiles"]=QJsonObject{{"active","radio-b"},{"items",items}};server.setSnapshot(state);bool done=false;QString error;client.startVoice([&](QString problem){error=problem;done=true;});QTRY_VERIFY(done);QVERIFY(error.contains("profile changed"));QCOMPARE(voices,0);
        QTRY_COMPARE(client.snapshot().value("profiles").toObject().value("active").toString(),QString("radio-b"));state["profiles"]=QJsonObject{{"active","radio-a"},{"items",items}};server.setSnapshot(state);QSignalSpy finished(&client,&remote::Client::transmitFinished);QVERIFY(client.sendWave(wave(),-1));QTRY_COMPARE(finished.count(),1);QVERIFY(finished[0][1].toString().contains("profile changed"));QCOMPARE(transmissions,0);
        // Commit is guarded too, even if an upload was already accepted.
        QTRY_COMPARE(client.snapshot().value("profiles").toObject().value("active").toString(),QString("radio-a"));auto* channel=client.findChild<remote::Channel*>();QVERIFY(channel);channel->send({{"type","tx.begin"},{"profile","radio-a"},{"id",900},{"tx",1},{"bytes",200},{"sha256",QString(64,'0')},{"morse",-1}});QTRY_VERIFY(server.remoteTransmitBusy());state["profiles"]=QJsonObject{{"active","radio-b"},{"items",items}};server.setSnapshot(state);channel->send({{"type","tx.commit"},{"profile","radio-a"},{"id",901}});QTRY_VERIFY(!server.remoteTransmitBusy());QCOMPARE(transmissions,0);
    }
    void configuration(){
        QSettings settings(directory_.filePath("remote.ini"),QSettings::IniFormat);remote::Config c;c.role=remote::Role::Client;c.host="shack.local";c.key=remote::newKey();QCOMPARE(c.key.size(),32);QVERIFY(c.validate().isEmpty());c.save(settings);const auto saved=remote::Config::load(settings);QCOMPARE(saved.host,c.host);QCOMPARE(saved.key,c.key);QCOMPARE(saved.role,c.role);
        c.key="123";QVERIFY(!c.validate().isEmpty());c.key=key_;c.host="bad host";QVERIFY(!c.validate().isEmpty());c.role=remote::Role::Local;QVERIFY(c.validate().isEmpty());
    }
    void fragmentedAndCoalescedFrames(){
        const QByteArray audio("\0\1\2\0\xff",5);const auto wire=remote::frame({{"type","pcm"}},audio)+remote::frame({{"type","stop"}});remote::Decoder decoder;std::vector<remote::Packet> packets;
        for(char byte:wire){auto batch=decoder.feed(QByteArray(1,byte));packets.insert(packets.end(),batch.begin(),batch.end());}
        QCOMPARE(packets.size(),size_t(2));QCOMPARE(packets[0].data,audio);QCOMPARE(packets[1].message.value("type").toString(),QString("stop"));
        remote::Decoder combined;QCOMPARE(combined.feed(wire).size(),size_t(2));
    }
    void malformedFramesAreBounded(){
        QByteArray huge(4,0);qToBigEndian<quint32>(remote::MaxFrameBytes+1,huge.data());remote::Decoder decoder;QVERIFY_EXCEPTION_THROWN(decoder.feed(huge),std::runtime_error);
        auto bytes=remote::frame({{"type","hello"}});qToBigEndian<quint32>(9999,bytes.data()+4);remote::Decoder invalid;QVERIFY_EXCEPTION_THROWN(invalid.feed(bytes),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(remote::frame({{"type","pcm"}},QByteArray(remote::MaxFrameBytes,'x')),std::runtime_error);
    }
    void authenticatedAudioAndRadioRequests(){
        remote::Server server;configure(server);server.setSnapshot({{"radioConnected",true},{"input","SignaLink"}});remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QVERIFY(server.hasClient());QCOMPARE(client.snapshot().value("input").toString(),QString("SignaLink"));
        QSignalSpy received(&client,&remote::Client::pcm);QByteArray audio(90001,'p');server.publishPcm(audio);QTRY_COMPARE(received.size(),3);QByteArray joined;for(const auto& event:received)joined+=event[0].toByteArray();QCOMPARE(joined,audio);
        connect(&server,&remote::Server::request,&server,[&](qint64 id,QString operation,QJsonObject arguments){QCOMPARE(operation,QString("radio.apply"));QCOMPARE(arguments.value("value").toDouble(),145550000.);server.reply(id);});bool done=false;client.command("radio.apply",{{"value",145550000.}},[&](QString error){QVERIFY(error.isEmpty());done=true;});QTRY_VERIFY(done);
    }
    void unchangedSnapshotsDoNotRefreshUiButRepliesAndPttStillArrive(){
        remote::Server server;configure(server);QJsonObject state{{"radioConnected",true},{"transmitting",false},{"input","SignaLink"}};server.setSnapshot(state);remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QSignalSpy updates(&client,&remote::Client::snapshotChanged),audio(&client,&remote::Client::pcm);
        for(int i=0;i<20;++i){server.setSnapshot(state);server.reply(1000+i);}
        server.publishPcm(QByteArray(2400,0));QTRY_COMPARE(audio.size(),1);QCOMPARE(updates.size(),0);
        connect(&server,&remote::Server::request,&server,[&](qint64 id,const QString&,const QJsonObject&){server.reply(id,{},{{"saved",true}});});
        bool done=false;client.commandResult("read",{},[&](QString error,QJsonObject result){QVERIFY(error.isEmpty());QVERIFY(result.value("saved").toBool());done=true;});QTRY_VERIFY(done);QCOMPARE(updates.size(),0);
        state["transmitting"]=true;server.setSnapshot(state);QTRY_COMPARE(updates.size(),1);QVERIFY(client.snapshot().value("transmitting").toBool());
        state["transmitting"]=false;server.setSnapshot(state);QTRY_COMPARE(updates.size(),2);QVERIFY(!client.snapshot().value("transmitting").toBool());
        client.disconnectFromHost();QTRY_VERIFY(!client.connected());QVERIFY(client.snapshot().isEmpty());
    }
    void wrongKeyNeverGrantsControl(){
        remote::Server server;configure(server);QSignalSpy stop(&server,&remote::Server::emergencyStop),commands(&server,&remote::Server::request);remote::Client client;pair(server,client,"ffffffffffffffffffffffffffffffff");QTest::qWait(100);QVERIFY(!client.connected());QVERIFY(!server.hasClient());QCOMPARE(stop.size(),0);QCOMPARE(commands.size(),0);
    }
    void failedStatusWriteCannotStartCancelledVoice(){
        remote::Server server;configure(server);server.setSnapshot({{"preparing",false}});
        remote::Client client;auto [shack,laptop]=remoteTestPair();server.acceptConnection(shack);client.connectDevice(laptop,key_);QTRY_VERIFY(client.connected());
        int starts=0;server.beginVoice=[&](remote::Server::Ready ready){++starts;ready({});};
        // MainWindow publishes a new snapshot synchronously when activity changes.
        connect(&server,&remote::Server::activityChanged,&server,[&]{server.setSnapshot({{"preparing",server.remoteTransmitBusy()}});});
        QSignalSpy stopped(&server,&remote::Server::emergencyStop);
        // Keep laptop-to-shack delivery working, but fail the status write back.
        // That closes the owner while voice.begin is still on the call stack.
        shack->peer.clear();client.startVoice([](QString){});
        QTRY_COMPARE(stopped.size(),1);QVERIFY(!server.hasClient());QVERIFY(!server.remoteTransmitBusy());QCOMPARE(starts,0);
        client.disconnectFromHost();
    }
    void resultRepliesPreserveSnapshotAndFailOnceOnDisconnect(){
        remote::Server server;configure(server);server.setSnapshot({{"input","shack-input"}});remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());
        connect(&server,&remote::Server::request,&server,[&](qint64 id,const QString& op,const QJsonObject&){if(op=="wizard-state")server.reply(id,{},{{"device","USB"}},server.session(),false);});
        bool received=false;client.commandResult("wizard-state",{},[&](QString error,QJsonObject result){QVERIFY(error.isEmpty());QCOMPARE(result.value("device").toString(),QString("USB"));received=true;});QTRY_VERIFY(received);QCOMPARE(client.snapshot().value("input").toString(),QString("shack-input"));
        int failures=0;client.commandResult("pending",{},[&](QString error,QJsonObject result){QVERIFY(!error.isEmpty());QVERIFY(result.isEmpty());++failures;});client.disconnectFromHost();QCOMPARE(failures,1);QTest::qWait(30);QCOMPARE(failures,1);
    }
    void forgedWelcomeNeverAuthenticatesServer(){
        auto [rogue,laptop]=remoteTestPair();remote::Channel server(rogue);remote::Client client;client.connectDevice(laptop,key_);server.send({{"type","welcome"},{"snapshot",QJsonObject{{"radioConnected",true}}}});QTest::qWait(50);QVERIFY(!client.connected());QVERIFY(client.status().contains("did not authenticate"));
    }
    void writeFailureDuringPairingAndDisconnectDoesNotCrash(){
        auto [rogue,laptop]=remoteTestPair();remote::Channel peer(rogue);remote::Client failed;laptop->peer.clear();failed.connectDevice(laptop,key_);peer.send({{"type","challenge"},{"version",1},{"nonce",QString(32,'0')}});QTest::qWait(50);QVERIFY(!failed.connected());QVERIFY(failed.status().contains("Could not send"));
        remote::Server server(nullptr,180);configure(server);remote::Client client;auto [shack,device]=remoteTestPair();server.acceptConnection(shack);client.connectDevice(device,key_);QTRY_VERIFY(client.connected());device->peer.clear();client.disconnectFromHost();QVERIFY(!client.connected());QTRY_VERIFY_WITH_TIMEOUT(!server.hasClient(),1500);
    }
    void oneControllerAndSessionIsolation(){
        remote::Server server;configure(server);remote::Client first;pair(server,first);QTRY_VERIFY(first.connected());const auto oldSession=server.session();remote::Client second;pair(server,second);QTest::qWait(150);QVERIFY(!second.connected());QVERIFY(first.connected());
        first.disconnectFromHost();QTRY_VERIFY(!server.hasClient());pair(server,second);QTRY_VERIFY(second.connected());QVERIFY(server.session()!=oldSession);QSignalSpy snapshot(&second,&remote::Client::snapshotChanged);server.reply(1,"old request",{},oldSession);QTest::qWait(30);QCOMPARE(snapshot.size(),0);
    }
    void completeWaveBeforeTransmission(){
        remote::Server server;configure(server);remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());const auto source=wave();QFile original(source);QVERIFY(original.open(QIODevice::ReadOnly));const auto expected=original.readAll();int transmissions=0;
        bool contentMatches=false,timingMatches=false;
        server.transmit=[&](const QString& path,qint64 duration,qint64 morse){QFile actual(path);contentMatches=actual.open(QIODevice::ReadOnly)&&actual.readAll()==expected;timingMatches=duration==2000&&morse== -1;++transmissions;server.setSnapshot({{"txDurationMs",double(duration)}});return QString{};};
        QSignalSpy started(&client,&remote::Client::transmitStarted),finished(&client,&remote::Client::transmitFinished);QString error;QVERIFY(client.sendWave(source,-1,&error));QCOMPARE(transmissions,0);QTRY_COMPARE(transmissions,1);QVERIFY(contentMatches);QVERIFY(timingMatches);QTRY_COMPARE(started.size(),1);QCOMPARE(started[0][0].toLongLong(),qint64(2000));QVERIFY(client.transmitting());server.finishTransmit(true,"Complete");QTRY_COMPARE(finished.size(),1);QVERIFY(!client.transmitting());
    }
    void invalidAndIncompleteAudioNeverTransmits(){
        remote::Server server;configure(server);int transmissions=0;server.transmit=[&](const QString&,qint64,qint64){++transmissions;return QString{};};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());const auto source=wave();
        QFile invalid(directory_.filePath("invalid.wav"));QVERIFY(invalid.open(QIODevice::WriteOnly));invalid.write("not audio");invalid.close();QString error;QVERIFY(!client.sendWave(invalid.fileName(),-1,&error));QVERIFY(!error.isEmpty());
        auto* channel=client.findChild<remote::Channel*>();QVERIFY(channel);channel->send({{"type","tx.begin"},{"id",900},{"tx",1},{"bytes",200},{"sha256",QString(64,'0')},{"morse",-1}});channel->send({{"type","tx.chunk"},{"id",901},{"offset",0}},QByteArray(100,'a'));channel->send({{"type","tx.commit"},{"id",902}});QTest::qWait(50);QCOMPARE(transmissions,0);
        QSignalSpy finished(&client,&remote::Client::transmitFinished);QVERIFY(client.sendWave(source,900000,&error));QTRY_COMPARE(finished.size(),1);QCOMPARE(transmissions,0);
    }
    void stopCancelsUploadAndDisconnectStopsStation(){
        remote::Server server;configure(server);int transmissions=0;server.transmit=[&](const QString&,qint64,qint64){++transmissions;return QString{};};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QSignalSpy stop(&server,&remote::Server::emergencyStop);
        QVERIFY(client.sendWave(wave(),-1));client.stopTransmit();QTRY_COMPARE(stop.size(),1);QVERIFY(!client.transmitting());QCOMPARE(transmissions,0);
        QVERIFY(client.sendWave(wave(),-1));QTRY_COMPARE(transmissions,1);client.disconnectFromHost();QTRY_VERIFY(!server.hasClient());QTRY_VERIFY(stop.size()>=2);QVERIFY(!client.transmitting());
    }
    void heartbeatLossStopsStation(){
        remote::Server server(nullptr,180);configure(server);remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QSignalSpy stop(&server,&remote::Server::emergencyStop);QTRY_COMPARE_WITH_TIMEOUT(stop.size(),1,1500);QTRY_VERIFY(!client.connected());
    }
    void shackStopCancelsUploadBeforePtt(){
        remote::Server server;configure(server);int transmissions=0;server.transmit=[&](const QString&,qint64,qint64){++transmissions;return QString{};};
        server.canTransmit=[&]{QTimer::singleShot(0,&server,[&]{server.abortTransmit("Shack operator pressed Stop");});return QString{};};
        remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QSignalSpy finished(&client,&remote::Client::transmitFinished);QVERIFY(client.sendWave(wave(480000),-1));QTRY_COMPARE(finished.size(),1);QVERIFY(!finished[0][0].toBool());QVERIFY(!client.transmitting());QCOMPARE(transmissions,0);QTest::qWait(100);QCOMPARE(transmissions,0);
    }
    void stopThenRetryIgnoresPreviousTransmissionCompletion(){
        remote::Server server;configure(server);int transmissions=0;server.transmit=[&](const QString&,qint64,qint64){++transmissions;return QString{};};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());const auto path=wave();QVERIFY(client.sendWave(path,-1));QTRY_COMPARE(transmissions,1);client.stopTransmit();QVERIFY(client.sendWave(path,-1));QTRY_COMPARE(transmissions,2);QVERIFY(client.transmitting());server.finishTransmit(true,"Second complete");QTRY_VERIFY(!client.transmitting());
    }
    void signedMessagesRejectTamperingAndReplay(){
        auto [senderDevice,receiverDevice]=remoteTestPair();remote::Channel sender(senderDevice),receiver(receiverDevice);sender.secure("test session key");receiver.secure("test session key");QSignalSpy packets(&receiver,&remote::Channel::packet),closed(&receiver,&remote::Channel::ended);
        QVERIFY(sender.send({{"type","stop"}}));QTRY_COMPARE(packets.size(),1);const auto replay=senderDevice->sent;senderDevice->write(replay);QTRY_COMPARE(closed.size(),1);QCOMPARE(packets.size(),1);
        auto [a,b]=remoteTestPair();remote::Channel left(a),right(b);left.secure("different key");right.secure("test session key");QSignalSpy rejected(&right,&remote::Channel::ended);left.send({{"type","command"},{"operation","radio.key"}});QTRY_COMPARE(rejected.size(),1);
    }
    void localAndRemoteTransmitReservationsExcludeEachOther(){
        remote::Server server;configure(server);int sends=0;server.transmit=[&](const QString&,qint64,qint64){++sends;return QString{};};remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());const auto path=wave();QSignalSpy finished(&client,&remote::Client::transmitFinished);
        QVERIFY(server.reserveLocalTransmit().isEmpty());QVERIFY(client.sendWave(path,-1));QTRY_COMPARE(finished.size(),1);QVERIFY(!finished[0][0].toBool());QCOMPARE(sends,0);server.releaseLocalTransmit();
        bool uploadRefusedLocal=false;connect(&server,&remote::Server::activityChanged,&server,[&]{if(server.remoteTransmitBusy())uploadRefusedLocal=!server.reserveLocalTransmit().isEmpty();});QVERIFY(client.sendWave(path,-1));QTRY_COMPARE(sends,1);QVERIFY(uploadRefusedLocal);QVERIFY(!server.reserveLocalTransmit().isEmpty());server.finishTransmit(true,"Complete");QTRY_VERIFY(!client.transmitting());QVERIFY(server.reserveLocalTransmit().isEmpty());server.releaseLocalTransmit();
    }
    void shackTransmitAudioHasAnIndependentReceiveStream(){
        remote::Server server;configure(server);remote::Client client;pair(server,client);QTRY_VERIFY(client.connected());QTest::qWait(30);QSignalSpy starts(&client,&remote::Client::monitorStarted),audio(&client,&remote::Client::monitorPcm),ends(&client,&remote::Client::monitorFinished),receiver(&client,&remote::Client::pcm);
        server.publishPcm(QByteArray(2400,'r'));server.startLocalMonitor();server.publishMonitorPcm(QByteArray(6000,'t'));server.publishMonitorPcm(QByteArray(9000,'x'));server.finishLocalMonitor(true);server.publishPcm(QByteArray(3600,'s'));
        QTRY_COMPARE(starts.size(),1);QTRY_COMPARE(audio.size(),2);QTRY_COMPARE(ends.size(),1);QTRY_COMPARE(receiver.size(),2);QCOMPARE(audio[0][0].toByteArray(),QByteArray(6000,'t'));QCOMPARE(audio[1][0].toByteArray(),QByteArray(9000,'x'));QVERIFY(ends[0][0].toBool());QVERIFY(client.connected());
        server.startLocalMonitor();server.publishMonitorPcm(QByteArray(1000,'n'));server.finishLocalMonitor(false);QTRY_COMPARE(starts.size(),2);QTRY_COMPARE(ends.size(),2);QVERIFY(!ends[1][0].toBool());
    }
    void tcpLoopbackWhenAvailable(){
        remote::Server server;if(!server.start(0,key_,QHostAddress::LocalHost))QSKIP(qPrintable("TCP loopback unavailable: "+server.error()));server.canTransmit=[]{return QString{};};remote::Config config;config.role=remote::Role::Client;config.host="127.0.0.1";config.key=key_;config.port=server.port();config.reconnect=false;remote::Client client;client.connectToHost(config);QTRY_VERIFY(client.connected());QSignalSpy pcm(&client,&remote::Client::pcm);server.publishPcm(QByteArray(2400,'x'));QTRY_COMPARE(pcm.size(),1);
    }
};
QTEST_GUILESS_MAIN(RemoteTests)
#include "remote_tests.moc"
