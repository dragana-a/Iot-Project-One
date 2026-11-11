const TIMEOUT = 1500;
const UPDATE_TIMER = 200;


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
    
            microphoneBar.max = data.decibelMode ? 90 : 5000;
    
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
            const maxDB = 60; // Adjustable reference value

            let level = Math.ceil(
                (Number(data.microphone) / maxDB) * 10
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
