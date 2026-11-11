// SPDX-License-Identifier: MIT
// F02/F03/F05 boundary fixtures. Included after the policy suite's Check helper.
#pragma once

bool FeatureNodeSame(const Node& a,const Node& b) {
    return a.last==b.last&&a.generation==b.generation&&a.state==b.state&&a.manual==b.manual&&
        a.began==b.began&&a.sticky==b.sticky&&a.presentation==b.presentation&&
        a.powerRequested==b.powerRequested&&a.stageBegan==b.stageBegan;
}
void FeaturePolicy() {
    std::string error;
    Config names,parsed;
    names.nicknames={{"A","Desk & meeting"},{"B","Desk & meeting"},{"disconnected", "\xe6\x9b\xb8\xe6\x96\x8e \xf0\x9f\x8c\x99"}};
    names.automatic=true;names.monitors["A"].hardware=true;names.monitors["A"].powerAfter=5;
    Profile profile;profile.name="Work";profile.policy=names;names.profiles.push_back(profile);
    auto serialized=Serialize(names);
    Check(serialized.starts_with("version=5\n")&&Parse(serialized,parsed,error)&&parsed.sourceSchema==5&&parsed.nicknames==names.nicknames&&Serialize(parsed)==serialized,"feature schema4 retains duplicate Unicode and disconnected nicknames with profiles");
    for(int version:{1,2,3}) {
        Config migrated;auto old="version="+std::to_string(version)+"\nmonitor.41=0,1\n";
        Check(Parse(old,migrated,error)&&migrated.sourceSchema==version&&migrated.nicknames.empty()&&!migrated.monitors.at("A").enabled&&!migrated.monitors.at("A").hardware,"feature old schemas retain defaults without nickname or new authority");
        Check(Parse(Serialize(migrated),parsed,error)&&parsed.sourceSchema==5,"feature old schemas serialize into schema4");
    }
    Check(ValidNickname("")&&ValidNickname("Desk & meeting")&&ValidNickname("\xe6\x9b\xb8\xe6\x96\x8e \xf0\x9f\x8c\x99"),"feature empty printable and non-Latin nicknames accepted");
    Config emptyName;Check(Parse("version=4\nnickname.41=\n",emptyName,error)&&emptyName.nicknames.empty(),"feature explicit empty imported name canonicalizes to fallback");
    auto nested="version=4\nprofile."+Hex("Work")+"=0|0|0|0||"+Hex("version=4\nnickname.41=4465736b\n")+"\n";
    Check(!Parse(nested,emptyName,error),"feature nested profile cannot override global device nickname");
    Config crowded;for(int i=0;i<65;++i)crowded.nicknames["named-"+std::to_string(i)]="Desk";
    Check(!Commit(emptyName,crowded,[](const std::string&){return true;}),"feature nickname count is bounded independently of connected topology");
    for(const auto& bad:std::vector<std::string>{"version=6\n","version=3\nnickname.41=4465736b\n","version=4\nnickname.41=4465736b\nnickname.41=4f74686572\n","version=4\nnickname.433a5c41=4465736b\nnickname.633a5c61=4f74686572\n","version=4\nnickname.zz=4465736b\n","version=4\nnickname.41=ff\n","version=4\nnickname.41=00\n","version=4\nnickname.41=0a\n"}) {
        auto previous=Serialize(parsed);Check(!Parse(bad,parsed,error)&&Serialize(parsed)==previous,"feature malformed forward legacy and duplicate name records fail transactionally");
    }
    std::string astral;for(int i=0;i<64;++i)astral+="\xf0\x9f\x8c\x99";
    Check(astral.size()==256&&ValidNickname(astral)&&!ValidNickname(astral+"a")&&!ValidNickname(std::string(65,'a')),"feature nickname byte and Unicode scalar bounds");
    const std::vector<std::string> invalidNames={std::string(1,'\0'),"line\nbreak","tab\tname",std::string(1,'\x7f'),"\xc2\x80","\xc2\x9f","\xe2\x80\xa8","\xe2\x80\xa9","\xe2\x80\xaa","\xe2\x80\xab","\xe2\x80\xac","\xe2\x80\xad","\xe2\x80\xae","\xe2\x81\xa6","\xe2\x81\xa7","\xe2\x81\xa8","\xe2\x81\xa9","\xc0\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82"};
    auto retained=Serialize(names);
    for(const auto& invalid:invalidNames) {
        Check(!ValidNickname(invalid),"feature nickname rejects controls bidi separators and malformed UTF8");
        auto next=names;next.nicknames["A"]=invalid;bool stored=false;
        Check(!Commit(names,next,[&](const std::string&){stored=true;return true;})&&!stored&&Serialize(names)==retained,"feature invalid nickname never reaches store or changes live configuration");
    }
    auto changed=names;changed.nicknames["A"]="Renamed";
    Check(!Commit(names,changed,[](const std::string&){return false;})&&Serialize(names)==retained,"feature failed nickname persistence retains live metadata and authority");
    Check(PolicyIdentity(static_cast<const Policy&>(names))==PolicyIdentity(static_cast<const Policy&>(changed)),"feature cosmetic metadata excluded from effective policy identity");
    DisarmImportedConfig(parsed=names);
    Check(parsed.nicknames==names.nicknames&&!parsed.automatic&&!parsed.monitors.at("A").hardware&&!parsed.monitors.at("A").powerAfter&&!parsed.profiles[0].policy.automatic&&!parsed.profiles[0].policy.monitors.at("A").hardware,"feature import keeps names while disarming base and profile authority");
    Controller rename;rename.config=names;rename.config.media=false;rename.config.timeout=5;rename.Topology({"A","B"},1);rename.SelectPolicy(1,0);rename.Tick(6001,{});
    auto renameNodes=rename.nodes;rename.config.nicknames["A"]="Another title";
    Check(!rename.SelectPolicy(6002,0)&&FeatureNodeSame(rename.nodes.at("A"),renameNodes.at("A"))&&FeatureNodeSame(rename.nodes.at("B"),renameNodes.at("B")),"feature cosmetic rename preserves all automatic sessions and idle histories");

    for(bool sharedInput:{false,true})for(bool sharedMedia:{false,true}) {
        Controller p;p.config.media=false;p.config.perInput=!sharedInput;p.config.perMedia=!sharedMedia;p.config.automatic=true;p.config.timeout=5;p.Topology({"A","B","C"},1);
        p.Tick(6001,{});p.Manual("B",6002,true,8);auto b=p.nodes.at("B"),c=p.nodes.at("C");
        Check(p.KeepAwake("A",7000,1)&&p.leases.at("A").deadline==67000&&!Running(p.nodes.at("A").state),"feature target lease stops its automatic presentation and resolves monotonic deadline");
        Check(FeatureNodeSame(p.nodes.at("B"),b)&&FeatureNodeSame(p.nodes.at("C"),c),"feature lease grant preserves manual and automatic peers under shared policies");
        auto explanation=p.Explain("A",{},7001);
        Check(explanation.primary==Reason::KeepAwake&&explanation.kind==DeadlineKind::KeepAwake&&explanation.remaining==59999,"feature explanation exposes target lease exact deadline");
        p.Tick(66999,{});
        Check(p.leases.contains("A")&&!Running(p.nodes.at("A").state)&&FeatureNodeSame(p.nodes.at("B"),b)&&FeatureNodeSame(p.nodes.at("C"),c),"feature lease holds until final monotonic millisecond without resetting peers");
        p.Tick(67000,{});
        Check(!p.leases.contains("A")&&p.nodes.at("A").last==67000&&!Running(p.nodes.at("A").state)&&FeatureNodeSame(p.nodes.at("B"),b)&&FeatureNodeSame(p.nodes.at("C"),c),"feature lease expiry gives only affected target a fresh idle interval");
        p.Tick(71999,{});Check(!Running(p.nodes.at("A").state),"feature expiry never immediately reactivates overdue target");
        p.Tick(72000,{});Check(Running(p.nodes.at("A").state),"feature fresh idle interval activates at exact post-lease boundary");
        Check(p.KeepAwake("A",73000,2)&&p.CancelKeepAwake("A",74000)&&!p.leases.contains("A")&&p.nodes.at("A").last==74000,"feature explicit cancellation clears target lease with fresh boundary");
        auto a=p.nodes.at("A");Check(!p.CancelKeepAwake("A",75000)&&FeatureNodeSame(p.nodes.at("A"),a)&&FeatureNodeSame(p.nodes.at("B"),b)&&FeatureNodeSame(p.nodes.at("C"),c),"feature nonexistent cancellation is a no-op and peers remain intact");
    }
    Controller replacement;replacement.config.media=false;replacement.config.automatic=true;replacement.config.timeout=5;replacement.Topology({"A","B","C"},1);replacement.Tick(6001,{});
    auto replacementB=replacement.nodes.at("B"),replacementC=replacement.nodes.at("C");
    Check(replacement.KeepAwake("A",7000,1)&&replacement.KeepAwakeUntil("A",8000,120000,"Replacement expiry")&&replacement.leases.size()==1&&replacement.leases.at("A").deadline==120000&&replacement.leases.at("A").resolvedExpiry=="Replacement expiry","feature new request replaces existing deadline and civil metadata rather than stacking leases");
    replacement.Tick(67000,{});Check(replacement.leases.contains("A"),"feature replaced earlier deadline cannot expire renewed lease");
    Check(replacement.KeepAwake("A",68000,1)&&replacement.leases.at("A").deadline==128000&&replacement.leases.at("A").resolvedExpiry.empty(),"feature duration replacement discards prior absolute-expiry caption");
    Check(replacement.KeepAwakeUntil("A",69000,70000,"Shorter expiry"),"feature explicit replacement can shorten existing lease");
    auto replacementA=replacement.nodes.at("A");
    Check(!replacement.KeepAwake("A",69001,0)&&replacement.leases.at("A").deadline==70000&&FeatureNodeSame(replacement.nodes.at("A"),replacementA),"feature invalid replacement preserves existing lease and node");
    replacement.Tick(70000,{});
    Check(replacement.leases.empty()&&replacement.nodes.at("A").last==70000&&FeatureNodeSame(replacement.nodes.at("B"),replacementB)&&FeatureNodeSame(replacement.nodes.at("C"),replacementC),"feature replacement expiry preserves all fields of nonmanual running peers");

    for(bool longLease:{false,true}) {
        Controller coexist;coexist.config.media=false;coexist.config.automatic=true;coexist.config.timeout=5;coexist.Topology({"A","B","C"},1);coexist.Manual("C",2,true,-1);auto stickyPeer=coexist.nodes.at("C");
        coexist.Snooze(1000,longLease?1:2);auto snoozeDeadline=coexist.snoozeUntil;
        Check(coexist.KeepAwake("A",2000,longLease?2:1)&&coexist.snoozeUntil==snoozeDeadline,"feature target lease creation does not alter shorter or longer global snooze");
        if(longLease) {
            coexist.Tick(61000,{});
            Check(!coexist.snoozeUntil&&coexist.leases.contains("A")&&coexist.Explain("A",{},61000).primary==Reason::KeepAwake,"feature shorter global snooze expiry leaves longer target lease active");
            coexist.Tick(66000,{});auto activePeer=coexist.nodes.at("B");Check(Running(activePeer.state),"feature global snooze release permits independent peer activation during longer target lease");
            coexist.Tick(122000,{});
            Check(!coexist.leases.contains("A")&&coexist.nodes.at("A").last==122000&&FeatureNodeSame(coexist.nodes.at("B"),activePeer),"feature longer target lease expiry preserves peer activated after snooze");
        }else {
            coexist.Tick(62000,{});
            Check(!coexist.leases.contains("A")&&coexist.snoozeUntil==121000&&coexist.Explain("A",{},62000).primary==Reason::Snoozed&&!coexist.nodes.at("A").manual&&!Running(coexist.nodes.at("A").state),"feature shorter target lease expiry leaves longer global snooze authoritative");
            coexist.Tick(121000,{});
            Check(!coexist.snoozeUntil&&coexist.nodes.at("A").last==121000&&!Running(coexist.nodes.at("A").state),"feature longer global snooze release establishes its own fresh idle boundary");
        }
        Check(FeatureNodeSame(coexist.nodes.at("C"),stickyPeer),"feature lease and snooze coexistence preserves manual peer completely");
        coexist.Snooze(200000,2);Check(coexist.KeepAwake("A",200001,1),"feature cancellation during global snooze fixture");
        Check(coexist.CancelKeepAwake("A",200002)&&coexist.snoozeUntil==320000&&!coexist.leases.contains("A")&&coexist.Explain("A",{},200002).primary==Reason::Snoozed&&FeatureNodeSame(coexist.nodes.at("C"),stickyPeer),"feature target cancellation during global snooze neither resumes automation nor alters sticky peer");
    }

    Controller attribution;attribution.config.media=false;attribution.config.automatic=true;attribution.config.timeout=5;attribution.Topology({"A","B","C"},1);attribution.Tick(6001,{});Check(attribution.KeepAwake("A",7000,2),"feature leased-input attribution fixture");
    auto inputB=attribution.nodes.at("B"),inputC=attribution.nodes.at("C");auto inputDeadline=attribution.leases.at("A").deadline;
    Check(attribution.Activity(8000,false,"A",{},true,8000,false)==1&&attribution.leases.at("A").deadline==inputDeadline&&FeatureNodeSame(attribution.nodes.at("B"),inputB)&&FeatureNodeSame(attribution.nodes.at("C"),inputC),"feature attributed input on leased target neither extends lease nor wakes independent peers");
    attribution.config.perInput=false;attribution.config.monitors["B"].input=1;attribution.config.monitors["C"].input=1;
    attribution.Input(9000,{},false,9000);
    Check(attribution.nodes.at("A").last==9000&&attribution.leases.at("A").deadline==inputDeadline&&FeatureNodeSame(attribution.nodes.at("B"),inputB)&&FeatureNodeSame(attribution.nodes.at("C"),inputC),"feature shared input on leased node preserves peers with explicit independent overrides");
    attribution.config.monitors["B"].input=0;attribution.Input(10000,{},false,10000);
    Check(!Running(attribution.nodes.at("B").state)&&FeatureNodeSame(attribution.nodes.at("C"),inputC)&&attribution.leases.at("A").deadline==inputDeadline,"feature explicit shared peer still accepts input while unrelated independent peer and lease remain intact");
    Controller bounds;bounds.config.media=false;bounds.Topology({"A","B"},1);
    auto unchanged=bounds.nodes;
    for(int minutes:{-1,0,1441,2147483647})Check(!bounds.KeepAwake("A",2,minutes)&&bounds.leases.empty()&&FeatureNodeSame(bounds.nodes.at("A"),unchanged.at("A")),"feature invalid lease durations are rejected without state mutation");
    Check(!bounds.KeepAwake("missing",2,1)&&!bounds.KeepAwake("@span",2,1)&&!bounds.KeepAwake("A",UINT64_MAX-1,1)&&bounds.leases.empty(),"feature unknown synthetic and overflow lease requests are rejected");
    bounds.config.monitors["A"].enabled=false;
    Check(!bounds.LeaseRejection("A").empty()&&!bounds.KeepAwake("A",2,1),"feature disabled display cannot acquire a lease");
    bounds.config.monitors["A"].enabled=true;bounds.unidentified.insert("A");
    Check(!bounds.KeepAwake("A",2,1),"feature unidentified display cannot acquire a lease");bounds.unidentified.clear();
    Check(bounds.KeepAwake("A",2,1440)&&bounds.leases.at("A").deadline==86400002,"feature maximum lease duration accepted without persisted monitor mutation");
    Check(Serialize(bounds.config).find("lease")==std::string::npos&&Serialize(bounds.config).find("86400002")==std::string::npos,"feature runtime deadline is absent from exported configuration");
    bounds.CancelKeepAwake("A",3);
    Check(!bounds.KeepAwakeUntil("A",4,4,"")&&!bounds.KeepAwakeUntil("A",4,3,"")&&bounds.leases.empty(),"feature absolute deadlines at or before now are rejected");
    Check(bounds.KeepAwakeUntil("A",5,60005,"2030-01-01 12:00")&&bounds.leases.at("A").resolvedExpiry=="2030-01-01 12:00","feature resolved civil expiry remains descriptive monotonic metadata");
    auto deadline=bounds.leases.at("A").deadline;bounds.Reset(10);
    Check(bounds.leases.at("A").deadline==deadline,"feature session reset preserves runtime lease deadline");
    bounds.Snooze(20,1);bounds.Tick(60020,{});
    Check(!bounds.leases.contains("A")&&!bounds.snoozeUntil,"feature global snooze and target leases expire independently");
    Check(bounds.KeepAwake("A",70000,1),"feature manual override lease fixture");bounds.Manual("A",70001,true,8);
    Check(bounds.leases.contains("A")&&bounds.nodes.at("A").manual&&bounds.nodes.at("A").sticky&&!bounds.KeepAwake("A",70002,1),"feature deliberate manual start overrides lease inhibitor and rejects replacing sticky presentation");
    Check(bounds.Explain("A",{},70002).primary==Reason::Sticky&&(bounds.Explain("A",{},70002).flags&(1u<<static_cast<unsigned>(Reason::KeepAwake))),"feature manual explanation preserves dormant lease flag without replacing authoritative state");
    auto manualLeaseNode=bounds.nodes.at("A");Check(bounds.CancelKeepAwake("A",70003)&&FeatureNodeSame(bounds.nodes.at("A"),manualLeaseNode),"feature cancelling dormant lease preserves active manual presentation");
    bounds.Reset(80000);bounds.nodes["@span"]={80000,++bounds.serial,State::Black,true,80000,true,-1};
    Check(bounds.Spanning()&&!bounds.LeaseRejection("A").empty()&&!bounds.KeepAwake("A",80001,1),"feature active manual span rejects target lease without disrupting span");
    bounds.nodes.erase("@span");Check(!bounds.Spanning(),"feature inactive span is not inferred from connected nodes");

    Controller topology;topology.config.media=false;topology.Topology({"A","B","C"},1);
    Check(topology.KeepAwake("A",2,10)&&topology.KeepAwake("B",2,10),"feature topology lease fixture");
    topology.Topology({"B","C"},10);
    Check(!topology.leases.contains("A")&&topology.leases.contains("B")&&topology.leases.at("B").deadline==600002,"feature topology drops disconnected lease without redirecting retained identity");
    topology.config.monitors["B"].enabled=false;topology.Tick(11,{});
    Check(topology.leases.empty(),"feature newly disabled target loses runtime lease");
    topology.config.monitors["B"].enabled=true;Check(topology.KeepAwake("B",12,1),"feature unidentified retirement fixture");topology.unidentified.insert("B");topology.Tick(13,{});
    Check(topology.leases.empty(),"feature newly unidentified target loses runtime lease");

    Controller selected;selected.config.media=false;selected.config.automatic=true;selected.config.timeout=5;selected.Topology({"A","B"},1);selected.SelectPolicy(1,0);
    Profile alternate;alternate.name="Alternate";alternate.policy=selected.config;alternate.policy.timeout=10;selected.config.profiles.push_back(alternate);
    Check(selected.KeepAwake("A",2,2),"feature profile retention fixture");auto selectedDeadline=selected.leases.at("A").deadline;
    selected.config.manualProfile="Alternate";selected.SelectPolicy(3,0);
    Check(selected.leases.at("A").deadline==selectedDeadline,"feature profile changes retain eligible runtime lease without extending it");
    selected.config.nicknames=names.nicknames;auto profileNames=selected.config.nicknames;
    selected.config.manualProfile.clear();selected.SelectPolicy(4,0);selected.config.manualProfile="Alternate";selected.SelectPolicy(5,0);
    Check(selected.config.nicknames==profileNames,"feature manual and automatic profile selections retain base nickname metadata");
    selected.Topology({"B"},6);Check(selected.config.nicknames==profileNames&&!selected.leases.contains("A"),"feature disconnected display retains its name while dropping runtime lease");
    selected.Topology({"B","A"},7);Check(selected.config.nicknames==profileNames&&selected.config.nicknames.at("A")==names.nicknames.at("A")&&!selected.leases.contains("A"),"feature reconnect and reorder restore same identity nickname without resurrecting lease");
    Check(selected.KeepAwakeUntil("A",8,selectedDeadline,"Retained deadline"),"feature restore monotonic fixture following reconnect");
    selected.blocked=true;selected.Reset(10);selected.Tick(100000,{});selected.blocked=false;selected.Reset(100001);selected.Tick(120001,{});
    Check(selected.leases.contains("A")&&!Running(selected.nodes.at("A").state),"feature suspend simulation retains unelapsed monotonic lease");
    selected.Tick(120002,{});
    Check(!selected.leases.contains("A")&&selected.nodes.at("A").last==120002,"feature suspend elapsed time retires lease at original monotonic deadline");
    puts("POLICY features PASS: schema4 names, privacy metadata, transactional validation, runtime leases, exact boundaries, span/manual/profile/topology and unchanged peers");
}
