const TIMEOUT = 1500;
const UPDATE_TIMER = 200;
const MAX_LOG_ENTRIES = 20;

function addLogEntry(data){
    const log = document.getElementById("dataLog");
    const entry = document.createElement("li");

    const time = new Date().toLocaleDateString();
    const unit = data.decibelMode ? "dB" : "Hz";
    const dial = data.resistorOk ? data.resistor + "%" : "--";
    const mic = data.micOk ? data.microphone + " " + unit : "--";

    entry.textContent = `${time} | Dial: ${dial} | Mic: ${mic}`;

    log.prepend(entry);

    while (log.children.length > MAX_LOG_ENTRIES) {
        log.lastElementChild.remove();
    }
}

function showStatus(message, isError){
    const status = document.getElementById("status");
    status.textContent = message;
    status.className = isError ? "error" : "ok";
}

async function fetchJSON(route){
    const control = new AbortController();
    const timer = setTimeout( () => control.abort(), TIMEOUT);

    try {
        const res = await fetch(route, {
            signal: control.signal,
            cache: "no-store"
        });
        if(!res.ok) throw new Error("HTTP " + res.status);
        return await res.json();
    } catch (error) {
        if(error.name === "AbortError"){
            throw new Error("Device unreachable: no response after " + (TIMEOUT / 1000) + " seconds");
        }
        if (error instanceof TypeError) {
            throw new Error("Network failure: could not connect to the ESP32");
        }
        if (error instanceof SyntaxError) {
            throw new Error("Device sent invalid data");
        }
        throw error;
    }
    finally{
        clearTimeout(timer);
    }
}

async function changeMicMode(){
    try {
        const res = await fetch("/changeMode", {cache: "no-store"});
        if(!res.ok) throw new Error("HTTP " + res.status);
    } catch (error) {
        showStatus("Could not change mode: " + error.message, true);
        console.error("Error changing mic:", error);
    }   
}


async function updateSensors() {
    try {

        const data = await fetchJSON("/data");
        addLogEntry(data);

        //store sensor failiures
        const problems = [];

        if (data.resistorOk) { 
            document.getElementById("resistor").textContent = data.resistor;
            document.getElementById("resistorBar").value = data.resistor;
        } else {
            document.getElementById("resistor").textContent = "--";
            problems.push("Dial reading 0: dial may be disconnected or turned fully down");
        }

        if (data.micOk) { 
            document.getElementById("microphone").textContent = data.microphone;
            const microphoneBar =
                document.getElementById("microphoneBar");
    
            microphoneBar.max = data.decibelMode ? 57 : 4000;
    
            microphoneBar.value = Math.max(
                0,
                Math.min(Number(data.microphone), microphoneBar.max)
            );
            
        } else {
            document.getElementById("microphone").textContent = "--";
            problems.push("Microphone Sensor failure (check wiring)");
        }


        document.getElementById("measurementType").textContent = (data.decibelMode) ? "dB" : "Hz";

        if(problems.length > 0){
            showStatus(problems.join(" | "), true);
        } else {
            showStatus("Connected", false);
        }


        const loudnessImage =
            document.getElementById("loudnessImage");

        const loudnessLevel =
            document.getElementById("loudnessLevel");

        if (data.decibelMode) {
            const maxDB = 57; 
            const minDB = 25;

            let level = Math.ceil(
                ((Number(data.microphone) - minDB) / (maxDB - minDB)) * 10   
            );

            level = Math.max(1, Math.min(10, level));

            loudnessLevel.textContent = level;

            loudnessImage.hidden = false;

            if (loudnessImage.dataset.level != level) {
                loudnessImage.src =
                    "/images/loudness-" + level + ".jpg";

                loudnessImage.dataset.level = level;
            }
        } else {
            loudnessLevel.textContent = "--";
            loudnessImage.hidden = true;
        }



    } catch (error) {
        showStatus(error.message, true);
        console.error("Error getting readings:", error);
    } finally {
        setTimeout(updateSensors, UPDATE_TIMER);
    }
}

updateSensors();
