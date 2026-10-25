# Google OAuth legal pages launch review

Draft policy pages are published from `public/legal/{privacy,terms}-{fa,en}.html`
through the C++ `HomeController::legalPage` route.

## Canonical addresses

- `https://hatef.ir/privacy` (Persian), `https://hatef.ir/privacy?lang=en` (English)
- `https://hatef.ir/terms` (Persian), `https://hatef.ir/terms?lang=en` (English)

Keep these addresses stable for the Google Auth Platform branding configuration.
The homepage must link to the *same* public privacy URL configured in the Google console.
No authentication should be needed to read them.

## Required checks before merging or submitting to Google

1. **Owner and contact**: confirm the legal operator of Hatef and a monitored support address; add full operator identity and physical contact details if required by applicable law.
2. **Data inventory**: confirm actual search/IP/analytics/profile data handling, subprocessors, security safeguards, cookie behavior, data locations and all relevant retention periods. Align published text with actual production behavior.
3. **OAuth flows**: verify `contacts.readonly` through People API and `gmail.send` through Gmail API are requested **incrementally** at the point of user need, not on ordinary login.
4. **Invitation consent**: confirm that the user can review both recipients and message before sending, that no unsolicited automation is active, and that sending rate limits, opt-outs, anti-abuse and recipient complaints are addressed.
5. **Stored data**: confirm precisely how contact data and access/refresh tokens are used, retained, secured, revoked and deleted. Implement and test disconnection and deletion flows. **Never publish unverified promises.**
6. **Google policy**: comply with [Google API Services User Data Policy](https://developers.google.com/terms/api-services-user-data-policy), including Limited Use restrictions and no advertising use of Google user data.
7. **Verification**: verify `hatef.ir` in Search Console, configure OAuth branding, add app scopes/justifications and an accurate demonstration video. A Testing status is not a production verification.
8. **Pages and SEO**: smoke test HTTP 200, `Content-Language`, correct `html[lang]`/`dir`, links and CSS under a production build. Make both links visibly accessible from the home page.
9. **Legal review**: validate terms, privacy policy, consumer rights, international requirements and effective date with qualified legal counsel before launch.

These texts are **drafts** pending the factual and legal checks above. No integration with Google APIs is implemented by this PR.
