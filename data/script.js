document.addEventListener('DOMContentLoaded', function() {
  const stateElement = document.getElementById('state');
  const switchButton = document.getElementById('switchButton');
  const switchToElement = document.getElementById('switchTo');
  const ipStateElement = document.getElementById('ipState');
  const discoveryStateElement = document.getElementById('discoveryState');
  const discoveryOnButton = document.getElementById('discoveryOn');
  const discoveryOffButton = document.getElementById('discoveryOff');

  function updateState() {
      fetch('/state')
          .then(response => response.text())
          .then(state => {
              stateElement.innerText = state;
              switchToElement.innerText = state === 'PC' ? 'Mac' : 'PC';
          });
  }

  function updateIPState() {
      fetch('/ip')
          .then(response => response.text())
          .then(ip => {
              ipStateElement.innerText = ip;
          });
  }

  function updateDiscoveryState() {
      fetch('/discovery_state')
          .then(response => response.text())
          .then(state => {
              discoveryStateElement.innerText = state;
          });
  }

  switchButton.addEventListener('click', function() {
      fetch('/switch', { method: 'POST' })
          .then(() => updateState());
  });

  discoveryOnButton.addEventListener('click', function() {
      fetch('/discovery_on', { method: 'POST' })
          .then(() => updateDiscoveryState());
  });

  discoveryOffButton.addEventListener('click', function() {
      fetch('/discovery_off', { method: 'POST' })
          .then(() => updateDiscoveryState());
  });

  updateState();
  updateIPState();
  updateDiscoveryState();
  setInterval(updateState, 2000);
});