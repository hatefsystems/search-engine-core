#include "../../include/search_engine/skills/SkillsData.h"

namespace search_engine {
namespace skills {

const std::vector<std::string> SkillsData::TECHNICAL_SKILLS = {
    // Programming Languages
    "C++", "Python", "JavaScript", "TypeScript", "Java", "C#", "Go", "Rust", "PHP", "Ruby",
    "Swift", "Kotlin", "Objective-C", "Scala", "R", "MATLAB", "Perl", "Lua", "Dart", "Elixir",
    
    // Web Technologies
    "HTML", "CSS", "React", "Vue.js", "Angular", "Node.js", "Express.js", "Next.js", "Nuxt.js",
    "jQuery", "Bootstrap", "Tailwind CSS", "Sass", "Less", "Webpack", "Vite", "GraphQL", "REST API",
    
    // Databases
    "MongoDB", "PostgreSQL", "MySQL", "Redis", "SQLite", "Oracle", "Microsoft SQL Server",
    "Cassandra", "DynamoDB", "Firebase", "Elasticsearch", "Neo4j", "CouchDB", "InfluxDB",
    
    // DevOps & Cloud
    "Docker", "Kubernetes", "AWS", "Azure", "Google Cloud", "CI/CD", "Jenkins", "GitLab CI",
    "GitHub Actions", "Terraform", "Ansible", "Chef", "Puppet", "Nginx", "Apache", "Linux",
    
    // Data Science & ML
    "Machine Learning", "Deep Learning", "TensorFlow", "PyTorch", "Scikit-learn", "Pandas",
    "NumPy", "Data Analysis", "Data Visualization", "Jupyter", "NLP", "Computer Vision",
    
    // Mobile Development
    "iOS Development", "Android Development", "React Native", "Flutter", "Xamarin", "Ionic",
    
    // Other Technical
    "Git", "Agile", "Scrum", "Microservices", "API Development", "Unit Testing", "TDD",
    "Software Architecture", "System Design", "Security", "Cryptography", "Blockchain",
    "IoT", "Embedded Systems", "Game Development", "Unity", "Unreal Engine"
};

const std::vector<std::string> SkillsData::BUSINESS_SKILLS = {
    // Management
    "Project Management", "Product Management", "Team Leadership", "Strategic Planning",
    "Business Strategy", "Operations Management", "Change Management", "Risk Management",
    "Stakeholder Management", "Budget Management", "Resource Planning", "Agile Management",
    
    // Marketing & Sales
    "Digital Marketing", "Content Marketing", "SEO", "SEM", "Social Media Marketing",
    "Email Marketing", "Brand Management", "Product Marketing", "Sales", "Business Development",
    "Lead Generation", "Customer Acquisition", "Marketing Analytics", "Growth Hacking",
    
    // Finance & Accounting
    "Financial Analysis", "Budgeting", "Forecasting", "Accounting", "Tax Planning",
    "Investment Analysis", "Financial Modeling", "Cost Management", "Auditing",
    
    // HR & Recruitment
    "Human Resources", "Recruitment", "Talent Acquisition", "Performance Management",
    "Employee Relations", "Training & Development", "Compensation & Benefits",
    
    // Communication
    "Public Speaking", "Presentation Skills", "Written Communication", "Interpersonal Skills",
    "Negotiation", "Conflict Resolution", "Client Relations", "Customer Service",
    
    // Analysis & Strategy
    "Business Analysis", "Data Analysis", "Market Research", "Competitive Analysis",
    "Requirements Gathering", "Process Improvement", "Lean Six Sigma", "Problem Solving"
};

const std::vector<std::string> SkillsData::CREATIVE_SKILLS = {
    // Design
    "Graphic Design", "UI/UX Design", "Web Design", "Mobile Design", "Logo Design",
    "Brand Design", "Typography", "Color Theory", "Layout Design", "Print Design",
    
    // Tools
    "Adobe Photoshop", "Adobe Illustrator", "Adobe InDesign", "Adobe XD", "Figma",
    "Sketch", "InVision", "Canva", "CorelDRAW", "GIMP", "Affinity Designer",
    
    // Media
    "Video Editing", "Motion Graphics", "Animation", "3D Modeling", "Photography",
    "Audio Editing", "Sound Design", "Illustration", "Digital Art", "Character Design",
    
    // Media Tools
    "Adobe Premiere Pro", "Adobe After Effects", "Final Cut Pro", "DaVinci Resolve",
    "Blender", "Maya", "Cinema 4D", "Lightroom", "Audacity", "Pro Tools",
    
    // Content
    "Content Writing", "Copywriting", "Technical Writing", "Creative Writing",
    "Blogging", "Storytelling", "Content Strategy", "Editing", "Proofreading",
    
    // Other Creative
    "Game Design", "Level Design", "UI Animation", "Prototyping", "Wireframing",
    "User Research", "Usability Testing", "Information Architecture"
};

std::vector<std::string> SkillsData::getAllSkills() {
    std::vector<std::string> allSkills;
    allSkills.reserve(TECHNICAL_SKILLS.size() + BUSINESS_SKILLS.size() + CREATIVE_SKILLS.size());
    
    allSkills.insert(allSkills.end(), TECHNICAL_SKILLS.begin(), TECHNICAL_SKILLS.end());
    allSkills.insert(allSkills.end(), BUSINESS_SKILLS.begin(), BUSINESS_SKILLS.end());
    allSkills.insert(allSkills.end(), CREATIVE_SKILLS.begin(), CREATIVE_SKILLS.end());
    
    return allSkills;
}

std::vector<std::string> SkillsData::getSkillsByCategory(const std::string& category) {
    if (category == "TECHNICAL") {
        return TECHNICAL_SKILLS;
    } else if (category == "BUSINESS") {
        return BUSINESS_SKILLS;
    } else if (category == "CREATIVE") {
        return CREATIVE_SKILLS;
    }
    return {};
}

} // namespace skills
} // namespace search_engine
