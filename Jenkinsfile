
pipeline {

    agent any

    options {
        timestamps()
        ansiColor('xterm')
        buildDiscarder logRotator(numToKeepStr: '100')
        disableConcurrentBuilds()
        timeout(activity: true, time: 1, unit: 'DAYS')
    }

    parameters {
        string defaultValue: 'master', description: 'Build scaffold Git branch', name: 'BRANCH', trim: true
        string defaultValue: 'master', description: 'QEMU Git branch', name: 'RELEASE_BRANCH', trim: true
        string defaultValue: 'balena-io', description: 'GitHub org', name: 'ACCOUNT', trim: true
        string defaultValue: 'qemu', description: 'GitHub repository', name: 'REPO', trim: true
        string defaultValue: '', description: '(e.g.) 7.0.0+balena1', name: 'QEMU_VERSION', trim: true
        string defaultValue: '', description: 'Git commit hash', name: 'RELEASE_COMMIT', trim: true
        credentials credentialType: 'org.jenkinsci.plugins.plaincredentials.impl.StringCredentialsImpl', defaultValue: '169db7d9-d27d-42ca-92fb-215663537c34', description: 'GitHub access token credentials', name: 'GIT_TOKEN_CREDENTIALS', required: true
        credentials credentialType: 'com.cloudbees.jenkins.plugins.sshcredentials.impl.BasicSSHUserPrivateKey', defaultValue: 'a2d8eaf4-a373-4efa-a9e3-c331a3687e72', description: 'GitHub SSH credentials', name: 'GIT_SSH_CREDENTIALS', required: true
    }

    stages {

        stage('scm') {
            steps {
                checkout([
                    $class: 'GitSCM',
                    branches: [[name: '*/${BRANCH}']],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [],
                    submoduleCfg: [],
                    userRemoteConfigs: [[
                        credentialsId: GIT_SSH_CREDENTIALS, 
                        url: 'git@github.com:${ACCOUNT}/${REPO}.git'
                    ]
                ]])
            }
        }

        stage('build') {
            steps {
                withCredentials([string(
                    credentialsId: GIT_TOKEN_CREDENTIALS,
                    variable: 'ACCESS_TOKEN'
                )]) {
                    sh returnStdout: true, script: 'automation/jenkins-build.sh'
                }
            }
        }
    }
}
