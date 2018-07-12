node {
    checkout scm
    def dockerImage
    stage('Build Docker Image') {
        def v8_version = sh(returnStdout: true, script: "make v8_version").trim()
        def commit = '7ef0cf169c47ebb0e107c801a1801d1a87978b40'
        script {
            def imageId = sh(returnStdout: true, script: "docker images -q wasm-c-api:${v8_version}").trim()
            if ("${imageId}" == "") {
                echo "No image for tag ${v8_version}. Building image."
                dockerImage = docker.build("wasm-c-api:${v8_version}")
            } else {
                echo "Found image ID ${imageId} for tag ${v8_version}. Skipping build."
                dockerImage = docker.image("wasm-c-api:${v8_version}")
            }
        }
    }
    stage('Build') {
        dockerImage.inside('-u root:root') {
            sh 'make'
        }
    }
}
