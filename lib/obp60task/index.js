// Add a new register card in web configuration interface
// This is a Java Script! 
(function(){
    const api=window.esp32nmea2k;
    if (! api) return;
    const tabName="Screen";
    api.registerListener((id, data) => {
    //    if (!data.testboard) return; //do nothing if we are not active
        let page = api.addTabPage(tabName, "Screen");
        api.addEl('button', '', page, 'Screenshot').addEventListener('click', function (ev) {
	window.open('/api/user/OBP60Task/screenshot', 'screenshot');
        })
    }, api.EVENTS.init);
})();
