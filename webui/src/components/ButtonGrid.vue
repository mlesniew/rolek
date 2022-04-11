<template>
  <div class="row justify-content-left align-items-center">
    <template v-if="config">
      <RolekButton
        v-for="(indices, name) in config"
        :key="name"
        :title="name"
        :indices="indices"
        :disabled="disabled"
        @request_start="$emit('request_start')"
        @request_success="$emit('request_success')"
        @request_failure="$emit('request_failure')"
        @request_end="$emit('request_end')"
      />
    </template>

    <template v-else-if="error">
      <div class="col-3"></div>
      <div class="col-6 text-center" role="alert">
        <p>An error occurred.</p>
        <button class="btn btn-secondary btn-small" @click="reload">
          Retry
        </button>
      </div>
    </template>

    <template v-else>
      <div class="col-12 text-center">
        <div class="spinner-border" role="status"></div>
      </div>
    </template>
  </div>
</template>

<script>
import axios from "axios";
import RolekButton from "./RolekButton.vue";

export default {
  components: {
    RolekButton,
  },

  emits: ["request_start", "request_success", "request_failure", "request_end"],

  props: {
    disabled: Boolean,
  },

  data() {
    return {
      config: null,
      error: false,
    };
  },

  mounted() {
    this.reload();
  },

  methods: {
    reload() {
      this.error = false;
      this.config = null;
      axios
        .get("config.json")
        .then((response) => {
          this.config = response.data;
        })
        .catch(() => {
          this.error = true;
        });
    },
  },
};
</script>
