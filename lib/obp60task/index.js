(function(){
    const api=window.esp32nmea2k;
    if (! api) return;
    const tabName="OBP60";
    api.registerListener((id, data) => {
    //    if (!data.testboard) return; //do nothing if we are not active
        let page = api.addTabPage(tabName, "Screenshot");
        api.addEl('button', '', page, 'Screenshot').addEventListener('click', function (ev) {
	window.open('/api/user/OBP60Task/screenshot', 'screenshot');
        })
    }, api.EVENTS.init);
})();
