# Security Policy

## Supported Versions

Currently supported versions for security updates:

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |

## Reporting a Vulnerability

We take security vulnerabilities seriously. If you discover a security issue, please follow these steps:

### 1. Do Not Disclose Publicly

Please do not open a public issue. Security vulnerabilities should be reported privately.

### 2. Report via Email

Send details to: [security@example.com] (replace with your actual security contact)

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

### 3. What to Expect

- **Acknowledgment**: Within 48 hours
- **Initial Assessment**: Within 5 business days
- **Status Updates**: Every 7 days until resolved
- **Fix Timeline**: Depends on severity
  - Critical: Within 7 days
  - High: Within 14 days
  - Medium: Within 30 days
  - Low: Next release cycle

### 4. Disclosure Process

1. Vulnerability is confirmed and fix is developed
2. Fix is tested and verified
3. Security advisory is prepared
4. Coordinated disclosure with reporter
5. Fix is released and advisory published
6. CVE assigned if applicable

## Security Considerations

### SQL Injection

The plugin uses PostgreSQL's query parser and does not execute raw SQL strings. However:

- Always validate outline names
- Use parameterized queries when creating outlines
- Be cautious with user-supplied hint content

### Privilege Escalation

- Creating outlines requires appropriate database privileges
- Outlines are stored in system tables with proper permissions
- Review outline permissions regularly

### Memory Safety

- Plugin uses PostgreSQL memory contexts
- No manual memory management
- Bounds checking on all string operations

### Input Validation

- Outline names are validated
- SQL text is parsed by PostgreSQL
- Hint content is sanitized

### Configuration Security

- Debug logging may expose query patterns
- Disable `pg_outline.debug_log` in production
- Limit access to outline management functions

## Best Practices

1. **Least Privilege**: Grant outline creation rights only to trusted users
2. **Regular Audits**: Review created outlines periodically
3. **Monitoring**: Monitor outline usage statistics
4. **Updates**: Keep plugin updated with latest security patches
5. **Testing**: Test in non-production before deploying

## Known Security Considerations

### 1. Outline Content

Outlines contain optimizer hints that affect query execution. Malicious hints could:
- Cause performance degradation
- Consume excessive resources
- Lead to denial of service

**Mitigation**: Restrict outline creation to trusted database administrators.

### 2. Information Disclosure

Outlines reveal information about:
- Query patterns
- Table structures
- Index names

**Mitigation**: Protect outline metadata with appropriate permissions.

### 3. Plan Stability vs Security

Forcing specific execution plans might:
- Bypass query optimizations
- Use outdated plans after schema changes
- Impact performance unexpectedly

**Mitigation**: Regular review and validation of outlines.

## Security Updates

Security updates are released as:
- Patch versions (1.0.x) for minor fixes
- Minor versions (1.x.0) for larger changes

Subscribe to:
- GitHub Security Advisories
- Release notifications
- Mailing list (if available)

## Questions?

For security-related questions (non-vulnerability):
- Open a discussion on GitHub
- Contact maintainers
- Check documentation

Thank you for helping keep PostgreSQL Outline Plugin secure!
