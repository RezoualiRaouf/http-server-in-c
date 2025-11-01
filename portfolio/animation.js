/**
 *  typing animation
 */

const typingTexts = [
  "A passionate computer science graduate",
  "Who is actively seeking internship opportunities!",
];

let currentTextIndex = 0;
let currentCharIndex = 0;
let isDeleting = false;
let isWaiting = false;

function typeText() {
  // Find the element - it should match your HTML structure
  const element = document.querySelector(".bio__description_text");

  if (!element) {
    console.error("Element with class bio__description_text not found");
    return;
  }

  const currentText = typingTexts[currentTextIndex];

  if (isWaiting) {
    // Wait period between delete and next text
    setTimeout(() => {
      isWaiting = false;
      typeText();
    }, 500);
    return;
  }

  if (!isDeleting) {
    // Typing phase
    element.textContent = currentText.substring(0, currentCharIndex + 1);
    currentCharIndex++;

    if (currentCharIndex === currentText.length) {
      // Finished typing, wait before deleting
      setTimeout(() => {
        isDeleting = true;
        typeText();
      }, 1000); // Wait 1 seconds before starting to delete
      return;
    }
  } else {
    // Deleting phase
    element.textContent = currentText.substring(0, currentCharIndex - 1);
    currentCharIndex--;

    if (currentCharIndex === 0) {
      // Finished deleting
      isDeleting = false;
      currentTextIndex = (currentTextIndex + 1) % typingTexts.length;
      isWaiting = true; // Add a small wait before starting next text
    }
  }

  // Set different speeds for typing vs deleting
  const speed = isDeleting ? 25 : 50;
  setTimeout(typeText, speed);
}

/**
 * navigation link highlighting
 */

function activeNavLink() {
  // get sections and navigation links
  const sections = document.querySelectorAll("section[id]");
  const navLinks = document.querySelectorAll(".header-nav-link");

  // watche elements and tell us when they enter/leave the viewport
  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        const sectionId = entry.target.getAttribute("id");

        if (entry.isIntersecting) {
          // This section is now visible
          updateActiveNavLink(sectionId);
        }
      });
    },
    {
      threshold: 0.3, // Trigger when 30% of section is visible
      rootMargin: "-50px 0px -50px 0px", // Adjust the trigger zone
    }
  );

  //  update active nav link style
  function updateActiveNavLink(activeSectionId) {
    navLinks.forEach((link) => {
      // Remove underline from ALL links first
      link.classList.remove("nav-underline");

      // Add underline ONLY to the link matching current section
      if (link.classList.contains(`nav__${activeSectionId}`)) {
        link.classList.add("nav-underline");
      }
    });
  }
  sections.forEach((section) => {
    observer.observe(section);
  });
}

/**
 * Back to top animation
 *
 */

function backToTop() {
  const topBtn = document.querySelector(".back_to_top");

  if (topBtn) {
    window.addEventListener("scroll", () => {
      if (window.scrollY > 400) {
        topBtn.classList.add("show");
      } else {
        topBtn.classList.remove("show");
      }
    });

    topBtn.addEventListener("click", () => {
      window.scrollTo({ top: 0, behavior: "smooth" });
    });
  } else {
    console.error("back to top button does not exist!");
  }
}
// wait until DOM loads
document.addEventListener("DOMContentLoaded", function () {
  setTimeout(typeText, 500);
  activeNavLink();
  backToTop();
});
