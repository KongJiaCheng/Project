// Parse Query Parameters
const urlParams = new URLSearchParams(window.location.search);
const username = urlParams.get('user') || 'Guest';
const goldAmount = parseInt(urlParams.get('gold')) || 100;
const price = parseFloat(urlParams.get('price')) || 0.99;
const tierName = urlParams.get('tier') || 'Handful of Gold';

// Populate Order Summary
document.getElementById('summary-tier').textContent = tierName;
document.getElementById('summary-price').textContent = `$${price.toFixed(2)}`;
document.getElementById('summary-gold').textContent = goldAmount.toLocaleString();
document.getElementById('summary-username').textContent = username;
document.getElementById('btn-text').textContent = `Pay $${price.toFixed(2)} Securely`;

// Receipt details populated in advance
document.getElementById('receipt-username').textContent = username;
document.getElementById('receipt-gold').textContent = `+${goldAmount.toLocaleString()} Gold`;

// Verification code generator (Cryptographically styled hash)
const secureHash = Math.random().toString(36).substring(2, 7).toUpperCase() + 
                   Math.random().toString(36).substring(2, 7).toUpperCase();
const verificationCode = `GOLD-${goldAmount}-${secureHash}`;
document.getElementById('verification-code').textContent = verificationCode;

// Dynamically generated Unique Transaction ID
const txId = 'TX-' + Math.floor(100000000 + Math.random() * 900000000);
document.getElementById('receipt-tx-id').textContent = txId;

// Card visual update triggers
const creditCard = document.getElementById('credit-card');
const cardNumberDisplay = document.getElementById('card-number-display');
const cardHolderDisplayVal = document.getElementById('card-holder-display-val');
const cardExpiryDisplayVal = document.getElementById('card-expiry-display-val');
const cvvDisplay = document.getElementById('cvv-display');
const cardBrand = document.getElementById('card-brand');
const inputCardIcon = document.getElementById('input-card-type-icon');

function updateCardHolder(val) {
    cardHolderDisplayVal.textContent = val.toUpperCase() || 'YOUR NAME';
}

function updateCVV(val) {
    cvvDisplay.textContent = val || '•••';
}

function flipCard(isFlipped) {
    if (isFlipped) {
        creditCard.classList.add('flipped');
    } else {
        creditCard.classList.remove('flipped');
    }
}

// Card number auto-spacing & type recognition
function formatCardNumber(input) {
    let value = input.value.replace(/\D/g, '');
    let formatted = '';
    
    // Auto-spacing every 4 digits
    for (let i = 0; i < value.length; i++) {
        if (i > 0 && i % 4 === 0) {
            formatted += ' ';
        }
        formatted += value[i];
    }
    input.value = formatted;
    cardNumberDisplay.textContent = formatted || '•••• •••• •••• ••••';

    // Simple Card Issuer Detection
    if (value.startsWith('4')) {
        cardBrand.textContent = 'VISA';
        inputCardIcon.textContent = '💳';
        cardBrand.style.color = '#00f2fe';
    } else if (/^5[1-5]/.test(value) || /^2[2-7]/.test(value)) {
        cardBrand.textContent = 'MC';
        inputCardIcon.textContent = '💳';
        cardBrand.style.color = '#ff6b6b';
    } else if (value.startsWith('34') || value.startsWith('37')) {
        cardBrand.textContent = 'AMEX';
        inputCardIcon.textContent = '💳';
        cardBrand.style.color = '#ffd200';
    } else if (value.startsWith('6011')) {
        cardBrand.textContent = 'DISC';
        inputCardIcon.textContent = '💳';
        cardBrand.style.color = '#ffa502';
    } else {
        cardBrand.textContent = 'CARD';
        inputCardIcon.textContent = '💳';
        cardBrand.style.color = '#ffffff';
    }
}

// Card expiry auto-slash
function formatExpiry(input) {
    let value = input.value.replace(/\D/g, '');
    if (value.length > 2) {
        input.value = value.substring(0, 2) + '/' + value.substring(2, 4);
    } else {
        input.value = value;
    }
    cardExpiryDisplayVal.textContent = input.value || 'MM/YY';
}

// Process payment submission
function processPayment(event) {
    event.preventDefault();
    
    const loaderOverlay = document.getElementById('loader-overlay');
    const successOverlay = document.getElementById('success-overlay');
    const loaderStatus = document.getElementById('loader-status');
    
    // Display Processing Screen
    loaderOverlay.style.display = 'flex';
    
    // Progression simulation steps for maximum premium checkout immersion!
    setTimeout(() => {
        loaderStatus.textContent = 'Verifying credentials...';
    }, 1000);

    setTimeout(() => {
        loaderStatus.textContent = 'Securing transaction gateway...';
    }, 2000);

    setTimeout(() => {
        loaderStatus.textContent = 'Approving transaction...';
    }, 3200);

    setTimeout(() => {
        loaderOverlay.style.display = 'none';
        successOverlay.style.display = 'flex';
        
        // Notify local server in background (if running)
        notifyGameEngine();
    }, 4500);
}

// Clipboard verification code utility
function copyVerificationCode() {
    const code = document.getElementById('verification-code').textContent;
    navigator.clipboard.writeText(code).then(() => {
        const copyBtn = document.querySelector('.copy-btn');
        copyBtn.textContent = 'Copied!';
        copyBtn.style.background = 'var(--success)';
        copyBtn.style.color = '#000';
        setTimeout(() => {
            copyBtn.textContent = 'Copy';
            copyBtn.style.background = 'rgba(255, 255, 255, 0.08)';
            copyBtn.style.color = 'var(--text-main)';
        }, 2000);
    });
}

// Micro-API: Inform local C++ game server of purchase completion
function notifyGameEngine() {
    const apiURL = `http://localhost:8888/success?user=${encodeURIComponent(username)}&gold=${goldAmount}&code=${verificationCode}`;
    
    fetch(apiURL, { mode: 'no-cors' })
        .then(() => console.log('Successfully informed local game server!'))
        .catch(err => console.warn('Local engine background listener not running or blocked, user can manually copy verification code.', err));
}
