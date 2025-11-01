window.addEventListener("load", function () {
  emailjs.init("6Ih25VGn6JauSMCGr");

  document
    .getElementById("contactForm")
    .addEventListener("submit", function (e) {
      e.preventDefault();

      // Check daily submission limit (also checks 24h lock)
      if (!canSubmitToday()) {
        showPopup("submitLimitPopup");
        return;
      }

      const submitBtn = document.getElementById("form__submit_btn");
      submitBtn.value = "Sending...";
      submitBtn.disabled = true;

      const formRef = this;
      emailjs
        .sendForm("service_a7b1l5w", "template_y20rm9c", formRef)
        .then(function (response) {
          if (response.status === 200) {
            recordSubmission(); // Record successful submission (and set lock if needed)
            updateSubmissionCounter(); // start the countdown timer if locked
            showPopup("thankYouPopup");
            document.getElementById("contactForm").reset();
          } else {
            throw new Error("EmailJS returned non-200 status");
          }
        })
        .catch(function (error) {
          console.error("Email send failed:", error);
          showPopup("errorPopup");
        })
        .finally(function () {
          submitBtn.value = "Send Message";
          submitBtn.disabled = false;
          // Always refresh UI after submission attempt
          updateSubmissionCounter();
        });
    });

  // Initialize tooltip updater
  startTooltipUpdater();

  // Optional: show/hide tooltip on trigger click
  document.querySelectorAll(".tooltip-trigger").forEach((btn) => {
    btn.addEventListener("click", function (e) {
      const container = btn.closest(".tooltip-container");
      if (!container) return;
      const content = container.querySelector(".tooltip-content");
      if (!content) return;
      content.style.display =
        content.style.display === "flex" ? "none" : "flex";
    });
  });
});

const LOCK_KEY = "submissionLock"; // stores timestamp (ms) when the lock ends
const DAILY_KEY = "dailySubmissions"; // stores object mapping dateString -> count
const MAX_PER_DAY = 2;

// Returns reset timestamp (ms) or null
function getLockResetTime() {
  const v = localStorage.getItem(LOCK_KEY);
  if (!v) return null;
  const parsed = parseInt(v, 10);
  if (isNaN(parsed)) return null;
  return parsed;
}

function setLockResetTime(timestampMs) {
  localStorage.setItem(LOCK_KEY, String(timestampMs));
}

function clearLock() {
  localStorage.removeItem(LOCK_KEY);
}

function canSubmitToday() {
  const now = Date.now();
  const lock = getLockResetTime();
  if (lock && now < lock) {
    // Still locked for 24h after using both submissions
    return false;
  } else if (lock && now >= lock) {
    // Lock expired, cleanup
    clearLock();
  }

  const today = new Date().toDateString();
  const submissions = JSON.parse(localStorage.getItem(DAILY_KEY) || "{}");

  // Remove old dates (keep only today)
  for (let date in submissions) {
    if (date !== today) delete submissions[date];
  }
  localStorage.setItem(DAILY_KEY, JSON.stringify(submissions));

  return (submissions[today] || 0) < MAX_PER_DAY;
}

function recordSubmission() {
  const today = new Date().toDateString();
  const submissions = JSON.parse(localStorage.getItem(DAILY_KEY) || "{}");
  submissions[today] = (submissions[today] || 0) + 1;

  // If we've reached the max, set a 24h lock starting now
  if (submissions[today] >= MAX_PER_DAY) {
    const resetMs = Date.now() + 24 * 60 * 60 * 1000;
    setLockResetTime(resetMs);
  }

  localStorage.setItem(DAILY_KEY, JSON.stringify(submissions));
}

let tooltipTimerId = null;

function updateSubmissionCounter() {
  // Updates both the "submission-counter" (if exists) and the tooltip texts
  const now = Date.now();
  const today = new Date().toDateString();
  const submissions = JSON.parse(localStorage.getItem(DAILY_KEY) || "{}");
  const used = submissions[today] || 0;
  const remaining = Math.max(0, MAX_PER_DAY - used);

  // Update counter element (if present)
  const counter = document.getElementById("submission-counter");
  if (counter) {
    counter.textContent = `Submissions remaining today: ${remaining}`;
    counter.style.color = remaining === 0 ? "red" : "";
  }

  // Update tooltip texts
  document.querySelectorAll(".tooltip-container").forEach((container) => {
    const descEl = container.querySelector(".tooltip-description");
    const footerEl = container.querySelector(".tooltip-footer-text");
    if (!descEl || !footerEl) return;

    const lock = getLockResetTime();

    if (lock && now < lock) {
      // Locked: show 0 left and time until reset
      descEl.textContent = `You have 0/${MAX_PER_DAY} submissions left`;
      const remainingMs = lock - now;

      // If >= 1 hour, show hours; if <1 hour show minutes
      if (remainingMs >= 60 * 60 * 1000) {
        const hours = Math.floor(remainingMs / (60 * 60 * 1000));
        footerEl.textContent = `restarts in ${hours}h`;
      } else {
        const mins = Math.ceil(remainingMs / 60000);
        footerEl.textContent = `restarts in ${mins}m`;
      }
    } else {
      // Not locked: show remaining
      descEl.textContent = `You have ${remaining}/${MAX_PER_DAY} submissions left`;
      footerEl.textContent = `restarts in 24h`;
    }
  });

  // Schedule next refresh when locked
  scheduleNextTooltipUpdate();
}

function scheduleNextTooltipUpdate() {
  // Clear previous timer and schedule next call based on remaining time
  if (tooltipTimerId) {
    clearTimeout(tooltipTimerId);
    tooltipTimerId = null;
  }

  const lock = getLockResetTime();
  if (!lock) {
    // Not locked -> schedule hourly check
    tooltipTimerId = setTimeout(updateSubmissionCounter, 60 * 60 * 1000);
    return;
  }

  const now = Date.now();
  if (now >= lock) {
    // Lock expired; refresh immediately
    tooltipTimerId = setTimeout(updateSubmissionCounter, 0);
    return;
  }

  const remainingMs = lock - now;

  if (remainingMs >= 60 * 60 * 1000) {
    // >= 1 hour left: update every hour at hour boundary
    const msToNextHour = remainingMs % (60 * 60 * 1000) || 60 * 60 * 1000;
    tooltipTimerId = setTimeout(updateSubmissionCounter, msToNextHour + 50);
  } else {
    // < 1 hour left: update every 5 minutes
    const interval = 5 * 60 * 1000;
    tooltipTimerId = setTimeout(updateSubmissionCounter, interval);
  }
}

function startTooltipUpdater() {
  // Called on load: update immediately and start scheduling
  updateSubmissionCounter();
}

// Show popup function
function showPopup(id) {
  document.getElementById(id).style.display = "flex";
}

// Close popup function
function closePopup() {
  document.querySelectorAll(".popup-overlay").forEach((el) => {
    el.style.display = "none";
  });
}

// Refresh on visibility change
document.addEventListener("visibilitychange", function () {
  if (document.visibilityState === "visible") {
    updateSubmissionCounter();
  }
});
