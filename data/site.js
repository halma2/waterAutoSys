let url = "ws://locsolás:1337/"
    var isConnected = false
    var websocket
    var activePin = -1
    window.addEventListener("load", wsConnect, false)
    function wsConnect() {
        websocket = new WebSocket(url)
        websocket.onopen = function() { onOpen() }
        websocket.onclose = function() { onClose() }
        websocket.onmessage = function(evt) { onMessage(evt) }
        websocket.onerror = function(evt) { console.log("SOCKET_ERROR: " + evt.data); }
    }
    function onOpen() {
        console.log("Connected")
        document.getElementById('offline').hidden = true
        isConnected = true
        
        console.log("Sending: ip")
        websocket.send("ip")
        console.log("Sending: zones")
        websocket.send("zones")
    }
    function onClose() {
        console.log("Disconnected");
        document.getElementById('offline').hidden = false
        isConnected = false;
        // Try to reconnect after a few seconds
        setTimeout(function() { wsConnect(url) }, 1000)
    }
    function onMessage(evt) {
        console.log("Received: " + evt.data);
        let msg;
        try {
            msg = JSON.parse(evt.data);
        } catch (e) {
            console.error("Invalid JSON:", evt.data);
            return;
        }
        switch (msg.type) {
        case "name":
            handleZoneNameUpdate(msg.pin, msg.name);
            break;
        case "zones":
            handleZonesList(msg.zones);
            break;
        case "off_zones":
            handleOffZonesList(msg.off_zones);
            break;
        case "zone-schedules":
            handleZoneSchedules(msg.pin, msg.schedules, msg.name);
            break;
        case "ip":
            handleIP(msg.ip);
            break;
        default:
            console.warn("Unknown message type:", msg.type);
        }
    }
    
    //TODO if other pinTimers[zone_x] == 0 => send isRunning = false
    function handleZonesList(zones) {
        console.log("Active zones:", zones);
        let zoneMenu = document.getElementsByTagName("nav")[0].children[0]
        Array.from(zoneMenu.querySelectorAll(".zone-btn")).forEach(el => el.remove());
        zones.forEach(zone => {
            const pin = zone.pin;
            const isRunning = zone.isRunning;
            let zBtn = document.createElement("li")
            zBtn.classList.add("zone-btn");
            zBtn.innerHTML ='<button onclick="changeZone(' + pin + ')">' + pin + '</button>'
            if (zone.turnedOn) {
                zBtn.classList.add("zone-ison")
            }
            if (zone.isRunning) {
                zBtn.classList.add("zone-running")
            }
            zoneMenu.prepend(zBtn)
        });
    }

    window.onclick = function(event) {
        if (!event.target.matches('.dropbtn')) {
            document.getElementById("new-zone-cont").style.display = "none"
        }
    }
    
    function handleOffZonesList(off_zones) {
        document.getElementById("new-zone-cont").innerHTML = ""
        document.getElementById("new-zone-cont").style.display = "block"
        console.log("Inactive zones:", off_zones);
        off_zones.forEach(element => {
            let zoneOpt = document.createElement("button")
            zoneOpt.innerHTML = element
            zoneOpt.onclick = () => createZone(element);
            document.getElementById("new-zone-cont").appendChild(zoneOpt)
        });
    }

    function handleZoneSchedules(pin, schedules, name) {
        console.log(`Schedules for zone ${pin}:`, schedules);
        let zoneMenu = document.getElementsByTagName("nav")[0].children[0]
        Array.from(zoneMenu.children).forEach(element => { 
            if (element.innerText == pin) {
                element.classList.add("active")
                activePin = element.innerText
            }
        });

        let now = new Date();
        let nowMinutes = now.getHours() * 60 + now.getMinutes();
        let nextSchedule = null;
        let minDelta = Infinity;
        schedules.forEach(schedule => {
            let schedMinutes = schedule.hour * 60 + schedule.minute;
            let delta = schedMinutes - nowMinutes;
            if (delta < 0) {
                delta += 24 * 60;  // Wrap to next day
            }
            if (delta < minDelta) {
                minDelta = delta;
                nextSchedule = schedule;
            }
        });
        if (nextSchedule){
            document.getElementById('nxt-h-m').innerText = nextSchedule.hour + ':' + nextSchedule.minute
        } else {
            document.getElementById('nxt-h-m').innerText = '-'
        }
        
        document.getElementById('act-zone-name').innerText = name 
        document.getElementById('zone-content').style.display = "block"
        document.getElementById('event-blocks').innerHTML = ''

        schedules.forEach(element => {
            let schedRecord = document.getElementById('event-block-sample').cloneNode(true)
            schedRecord.querySelector('.st-h')[0].innerText = element.hour
            schedRecord.querySelector('.st-min')[0].innerText = element.minute
            schedRecord.querySelector('act_min_slide_char')[0].innerText = element.duration
            schedRecord.querySelector('slide-prev-val')[0].innerText = element.duration
            f5EndClock(schedRecord)
        });

        /**Sorba rendezés*///TODO this to handleSchedule
        const evBlockCont = document.getElementById('event-blocks')
        const divs = evBlockCont.children
        if (divs.length > 1) {
            for (let i = 0; i < divs.length - 1; i++) {
                if (parseInt(divs[i].querySelector('.st-h').innerText) > parseInt(thgrgrPar.querySelector('.st-h').innerText) ||
                    (parseInt(divs[i].querySelector('.st-h').innerText) === parseInt(thgrgrPar.querySelector('.st-h').innerText) &&
                    parseInt(divs[i].querySelector('.st-min').innerText) > parseInt(thgrgrPar.querySelector('.st-min').innerText))){
                    evBlockCont.insertBefore(thgrgrPar.parentElement, divs[i])
                    break
                }
            }
        }
    }
    
    function handleZoneNameUpdate(pin, name) {
        console.log(`Zone ${pin} is named: ${name}`);
        document.getElementById("act-zone-name").innerText = name
    }

    function selectNewZones(){
        if (isConnected) {
            console.log("Sending: new_zones")
            websocket.send("new_zones")
        }
    }
    
    function changeZone(num) {
        if (isConnected) {
            if (document.getElementsByTagName('nav')[0].children[0].children[0].classList.contains('erasable')) {
                console.log("Sending: z_del." + num)
                websocket.send("z_del."+num)
                console.log("Sending: zones")
                websocket.send("zones")
                if (num == activePin){
                    activePin = -1
                    document.getElementById('event-blocks').innerHTML = ''
                    document.getElementById('zone-content').style.display = 'none'
                }
                else { //TODO biztos? lehetne a isrunnning, active külön
                    console.log("Sending: get." + activePin)
                    websocket.send("get." + activePin)
                } 
            }
            else {
                console.log("Sending: zones")
                websocket.send("zones")
                console.log("Sending: get." + num)
                websocket.send("get." + num)
            }           
        }
    }

    function createZone(pin) {
        document.getElementById("new-zone-cont").style.display = "none";
        if (isConnected){
            console.log("Sending: z_new." + pin)
            websocket.send("z_new."+pin)
            changeZone(pin)
        }
    }

    function editZones() {
        let editButton = document.getElementById('edit-zone').innerText
        let zonesParent = document.getElementsByTagName('nav')[0].children[0]
        if (editButton == "OK") {
            for (let index = 0; index < zonesParent.children.length - 1; index++) {
                zonesParent.children[index].classList.add('erasable')
            }
            editButton = "OK"
        }
        else {
            for (let index = 0; index < zonesParent.children.length - 1; index++) {
                zonesParent.children[index].classList.remove('erasable')
            }
            editButton = "Töröl"
        }
    }

    function editZoneName() {
        document.querySelectorAll('.name-edit-hid').forEach(e => e.style.display = "block")
        document.querySelectorAll('.name-edit-show').forEach(e => e.style.display = "none")
    }

    function sendEditZoneName() {
        if (isConnected) {
            websocket.send('nm.'+activePin+'.'+document.getElementById('name-input').value)
        }
        document.querySelectorAll('.name-edit-hid').forEach(e => e.style.display = "none")
        document.querySelectorAll('.name-edit-show').forEach(e => e.style.display = "block")
    }

    function zoneOff() {
        if (isConnected) {
            let zoneOffBtn = document.getElementById('turrn-off-btn')
            if (zoneOffBtn.innerText == 'ON') {
                zoneOffBtn.innerText = 'OFF'
                console.log("Sending: z_off."+activePin)
                websocket.send("z_off."+activePin)
                zoneOffBtn.classList.remove('zone-ison')
            } else {
                zoneOffBtn.innerText = 'ON'
                console.log("Sending: z_on."+activePin)
                websocket.send("z_on."+activePin)
                zoneOffBtn.classList.add('zone-ison')
            }
        }
    }

    function handleIP(ip) {
        document.getElementById('ip-adr').innerText = ip
    }


    let timerIV
    function runTimer() {//<!--TODO name+átnevezés STOPRA??-->
        if (this.innerText === 'RUN') {//TODO ajax: isRunning-zone->villog és minden időzítőt stop mikor az fut(na)
            document.getElementById('d-counter-min').innerText = document.getElementById('slider_val_char').value
            let counterC = document.getElementById('d-counter-min').innerText
            this.innerText = 'STOP'
            if (isConnected && activePin > -1) {
                console.log('sending: t_on.'+ activePin + '.' + counterC)
                websocket.send('t_on.'+ activePin + '.' + counterC)
            }
            timerIV = window.setInterval(function (){
                if (counterC > 0){
                    counterC = parseInt(counterC) - 1
                    document.getElementById('d-counter-min').innerText = counterC
                } else
                    clearInterval(timerIV)
            },1000) // TODO -> 60000

        } else {
            if (isConnected && activePin > -1) {
                websocket.send('t_off.'+ activePin)
            }
            clearInterval(timerIV)
            this.innerText = 'RUN'
        }
    }

    let schdeIndex = 0;
    function createSchedule(){
        this.disabled = true
        document.querySelectorAll('.edit-btn').forEach(btn => {btn.disabled = true; });
        let newBlock = (document.getElementById('event-block-sample').children[0].cloneNode(true))
        newBlock.id = "act-block" + schdeIndex++;
        newBlock.querySelector('.act_min_slide').value = 1
        newBlock.querySelector('.act_min_slide_char').value = 1
        document.getElementById('event-blocks').appendChild(newBlock)
    }

    function f5EndClock(mPar) { // calculate and refresh the end-time from the duration
        let endTime = 60*parseInt(mPar.querySelector('.st-h').innerText)
            + parseInt(mPar.querySelector('.st-min').innerText)
            + parseInt(mPar.parentElement.querySelector('.act_min_slide_char').innerText)
        let ora = Math.floor(endTime / 60)
        if (ora > 23) {//TODO slider+char-t mozgat
            mPar.querySelector('.end-h').innerText = '23'
            mPar.querySelector('.end-min').innerText = '59'
        } else {
            mPar.querySelector('.end-h').innerText = ora
            mPar.querySelector('.end-min').innerText = endTime % 60
        }
        mPar.parentElement.querySelector('.edit-btn').disabled = false
    }

    let origsHour
    let origsMin
    let origduratMin
    function editSchedule(button) {
        let thgrgrPar = button.parentNode.parentNode.parentElement
        if (button.innerHTML === 'EDIT') { //TODO lecserélnni, pl. val->kép???
            document.getElementById('new-act-btn').disabled = true
            thgrgrPar.classList.add('is-editing')
            button.innerHTML = 'OK'
            origsHour = thgrgrPar.getElementsByClassName('st-h')[0].innerText
            origsMin = thgrgrPar.getElementsByClassName('st-min')[0].innerText
            origduratMin = thgrgrPar.getElementsByClassName('act_min_slide_char')[0].innerText
        } else {
            //if not editted
            //websocket.send(activePin+'.'++'.'+'.')
            //TODO '-' nem lehet az órában!!!
            //TODO előtte ajax üzi a szervernek erről a változásról
            //TODO if okay... else err+return

            //TODO ajax szervernél is rendez/csak a javitás helyét küldi el
            let sHour = thgrgrPar.getElementsByClassName('st-h')[0].innerText
            let sMin = thgrgrPar.getElementsByClassName('st-min')[0].innerText
            let duratMin = thgrgrPar.getElementsByClassName('act_min_slide_char')[0].innerText

            if (isConnected) {

                if (sHour != origsHour || sMin != origsMin || duratMin != origduratMin) {
                    websocket.send('se.'+activePin+'.'+sHour+'.'+sMin+'.'+duratMin+'.'+origsHour+'.'+origsMin+'.'+origduratMin)
                }
                else {
                    websocket.send('sn.'+activePin+'.'+sHour+'.'+sMin+'.'+duratMin)
                }
                websocket.send('get.'+activePin)
            }
            thgrgrPar.classList.remove('is-editing')
            button.innerHTML = 'EDIT'
            document.getElementById('new-act-btn').disabled = false
            document.querySelectorAll('.edit-btn').forEach(btn => {btn.disabled = false; });
        }
    }

    function deleteSchedule(button) {
        let thgrgrPar = button.parentNode.parentNode.parentElement
        let sHour = thgrgrPar.getElementsByClassName('st-h')[0].innerText
        let sMin = thgrgrPar.getElementsByClassName('st-min')[0].innerText
        button.parentNode.parentNode.parentNode.parentElement.remove()
        document.getElementById('new-act-btn').disabled = false
        if (isConnected) {
            websocket.send('s_del.'+activePin+'.'+sHour+'.'+sMin)//todo bef
        }
    }