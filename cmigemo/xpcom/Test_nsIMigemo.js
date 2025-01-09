// Copyright 2005 MURAOKA Taro(KoRoN)/KaoriYa

function test_Migemo()
{
    var migemo, answer;
    try {
	netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	const cid = "@kaoriya.net/migemo/nsMigemo;1"
	migemo = Components.classes[cid].createInstance();
	migemo = migemo.QueryInterface(Components.interfaces.nsIMigemo);
    } catch (err) {
	alert("ERROR:1: " + err);
	return;
    }
    try {
	answer = migemo.query(document.getElementById("MIGEMO_INPUT").value);
	document.getElementById("MIGEMO_OUTPUT").value = answer;
    } catch (err) {
	alert("ERROR:2: " + err);
	return;
    }
}
