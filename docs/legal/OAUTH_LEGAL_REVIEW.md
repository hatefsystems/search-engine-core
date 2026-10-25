# Google OAuth Legal Page Review

## URLs

- Persian privacy: `https://hatef.ir/privacy`
- English privacy: `https://hatef.ir/privacy?lang=en`
- Persian terms: `https://hatef.ir/terms`
- English terms: `https://hatef.ir/terms?lang=en`

## Before Publication

- Verify the legal operator identity and monitored support email.
- Document collected data categories and infrastructure providers.
- Confirm data retention periods and applicable deletion rules.
- Validate the storage, security and revocation of OAuth tokens.
- Avoid unnecessarily retaining complete Google contact lists.
- Request `contacts.readonly` only for the contact picker.
- Request `gmail.send` only when the user enables sending.
- Show recipients and message contents before sending.
- Require new consent for any future scheduled-send feature.
- Apply anti-spam controls, limits and recipient opt-outs.
- Comply with Google's Limited Use requirements.
- Review policies with qualified legal counsel.
- Verify the domain in Google Search Console.
- Complete OAuth scope verification before production launch.
- Test both languages, HTTP 200, links, CSS and response headers.
- Match the homepage privacy URL with the Google Console URL.

## Scope

This PR implements legal pages, not Google API integrations.
The drafts must match production behavior before publication.
